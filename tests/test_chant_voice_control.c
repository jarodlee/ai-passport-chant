#include "chant_voice_control.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    chant_voice_control_t c={0};
    assert(!chant_voice_control_busy(&c));
    assert(!chant_voice_control_begin(&c));
    chant_voice_control_press(&c,true);
    assert(chant_voice_control_requested(&c) && chant_voice_control_held(&c));
    assert(chant_voice_control_begin(&c));
    assert(!chant_voice_control_requested(&c) && !chant_voice_control_cancelled(&c));
    /* A duplicate down event cannot interrupt the recording it started. */
    chant_voice_control_press(&c,true);
    assert(!chant_voice_control_requested(&c) && !chant_voice_control_cancelled(&c));
    chant_voice_control_press(&c,false); /* Upload / reply follows release. */
    assert(chant_voice_control_busy(&c) && !chant_voice_control_held(&c));
    chant_voice_control_press(&c,true); /* Barge in during reply. */
    assert(chant_voice_control_cancelled(&c) && chant_voice_control_requested(&c));
    chant_voice_control_finish(&c); /* Old transaction must preserve new press. */
    assert(chant_voice_control_busy(&c) && chant_voice_control_requested(&c));
    assert(chant_voice_control_begin(&c));
    assert(chant_voice_control_held(&c) && !chant_voice_control_cancelled(&c));
    chant_voice_control_press(&c,false);
    chant_voice_control_finish(&c);
    assert(!chant_voice_control_busy(&c));
    /* A press after finish is also kept; a quick release never forces a mic. */
    chant_voice_control_press(&c,true);
    chant_voice_control_press(&c,false);
    assert(chant_voice_control_begin(&c) && !chant_voice_control_held(&c));
    chant_voice_control_finish(&c);
    chant_voice_control_press(&c,true);
    assert(chant_voice_control_begin(&c));
    chant_voice_control_press(&c,false);
    chant_voice_control_press(&c,true);
    chant_voice_control_cancel(&c); /* Back discards a pending restart as well. */
    assert(!chant_voice_control_held(&c) && !chant_voice_control_requested(&c));
    assert(chant_voice_control_cancelled(&c));
    chant_voice_control_finish(&c);
    assert(!chant_voice_control_busy(&c) && !chant_voice_control_begin(&c));
    chant_voice_control_press(&c,true);
    assert(chant_voice_control_begin(&c) && !chant_voice_control_cancelled(&c));
    puts("Voice barge-in, held key, cancellation and handoff: PASS");
}
