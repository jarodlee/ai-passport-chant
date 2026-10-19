#include "chant_voice_control.h"
enum { HELD=1, PENDING=2, BUSY=4, CANCELLED=8 };

void chant_voice_control_press(chant_voice_control_t *s, bool down) {
    unsigned previous=atomic_load(&s->flags), next;
    do {
        if (down && (previous & HELD)) return;
        next=down ? previous | HELD | PENDING : previous & ~HELD;
        if (down && (previous & BUSY)) next |= CANCELLED;
    } while (!atomic_compare_exchange_weak(&s->flags, &previous, next));
}
void chant_voice_control_cancel(chant_voice_control_t *s) {
    unsigned previous=atomic_load(&s->flags), next;
    do { next=(previous | CANCELLED) & ~(HELD | PENDING); }
    while (!atomic_compare_exchange_weak(&s->flags, &previous, next));
}
bool chant_voice_control_begin(chant_voice_control_t *s) {
    unsigned previous=atomic_load(&s->flags), next;
    do {
        if (!(previous & PENDING)) return false;
        next=(previous | BUSY) & ~(PENDING | CANCELLED);
    } while (!atomic_compare_exchange_weak(&s->flags, &previous, next));
    return true;
}
void chant_voice_control_finish(chant_voice_control_t *s) {
    /* Never erase a press that arrived while the previous reply was running. */
    atomic_fetch_and(&s->flags, ~BUSY);
}
bool chant_voice_control_requested(const chant_voice_control_t *s) { return (atomic_load(&s->flags) & PENDING)!=0; }
bool chant_voice_control_busy(const chant_voice_control_t *s) { return (atomic_load(&s->flags) & (BUSY | PENDING))!=0; }
bool chant_voice_control_held(const chant_voice_control_t *s) { return (atomic_load(&s->flags) & HELD)!=0; }
bool chant_voice_control_cancelled(const chant_voice_control_t *s) { return (atomic_load(&s->flags) & CANCELLED)!=0; }
