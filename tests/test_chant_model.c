#include "chant_model.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    chant_model_t s; chant_model_init(&s);
    assert(s.playing && s.volume == 40 && s.voice_volume == 80 && !s.failed);
    for (int i = 0; i < 5; i++) chant_model_command(&s, CHANT_VOICE_UP);
    assert(s.voice_volume == 100);
    for (int i = 0; i < 20; i++) chant_model_command(&s, CHANT_VOICE_DOWN);
    assert(s.voice_volume == 20);
    chant_model_command(&s, CHANT_VOICE_TEST);
    assert(s.voice_volume == 20);
    assert(!chant_model_advance(&s, 9, 10) && s.position == 9);
    chant_model_command(&s, CHANT_TOGGLE);
    assert(!chant_model_advance(&s, 4, 10) && s.position == 9);
    chant_model_command(&s, CHANT_TOGGLE);
    assert(chant_model_advance(&s, 1, 10) && s.position == 0 && s.completed == 1);
    for (int i = 0; i < 40; i++) chant_model_command(&s, CHANT_UP);
    assert(s.volume == 90);
    for (int i = 0; i < 40; i++) chant_model_command(&s, CHANT_DOWN);
    assert(s.volume == 0);
    s.failed = true; assert(!chant_model_advance(&s, 5, 10));
    chant_model_command(&s, CHANT_RESTART);
    assert(s.playing && !s.failed && s.position == 0 && s.completed == 0 && s.volume == 0);
    assert(!chant_model_advance(&s, 1, 0));
    assert(s.track == 0 && !s.sequence);
    chant_model_command(&s, CHANT_SELECT_2);
    assert(s.track == 2 && s.position == 0 && s.playing && !s.failed);
    assert(chant_model_advance(&s, 10, 10) && s.track == 2);
    chant_model_command(&s, CHANT_SEQUENCE);
    assert(chant_model_advance(&s, 10, 10) && s.track == 3);
    assert(chant_model_advance(&s, 10, 10) && s.track == 4);
    assert(chant_model_advance(&s, 10, 10) && s.track == 0);
    assert(chant_model_advance(&s, 10, 10) && s.track == 1);
    chant_model_command(&s, CHANT_SELECT_0);
    assert(s.track == 0 && s.completed == 0);
    s.failed = true;
    chant_model_command(&s, CHANT_SELECT_1);
    assert(s.track == 1 && !s.failed && s.playing);
    chant_model_command(&s, CHANT_SELECT_3);
    assert(s.track == 3 && s.position == 0 && s.playing);
    chant_model_command(&s, CHANT_SELECT_4);
    assert(s.track == 4 && s.position == 0 && s.playing);
    puts("Chant state/playlist tests: PASS");
}
