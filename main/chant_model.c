#include "chant_model.h"
#include "chant_playlist.h"
void chant_model_init(chant_model_t *s) {
    *s = (chant_model_t){.volume = 80, .voice_volume = 80, .track = 4, .playing = true};
}
void chant_model_command(chant_model_t *s, chant_command_t command) {
    switch (command) {
        case CHANT_TOGGLE: if (!s->failed) s->playing = !s->playing; break;
        case CHANT_UP: if (s->volume < 90) s->volume += 5; break;
        case CHANT_DOWN: if (s->volume >= 5) s->volume -= 5; break;
        case CHANT_SEQUENCE: s->sequence = !s->sequence; break;
        case CHANT_VOICE_UP: if (s->voice_volume <= 90) s->voice_volume += 10; break;
        case CHANT_VOICE_DOWN: if (s->voice_volume >= 30) s->voice_volume -= 10; break;
        case CHANT_VOICE_TEST: break;
        case CHANT_SELECT_0: case CHANT_SELECT_1: case CHANT_SELECT_2: case CHANT_SELECT_3: case CHANT_SELECT_4:
            s->track = command - CHANT_SELECT_0;
            s->position = 0; s->completed = 0; s->playing = true; s->failed = false;
            break;
        case CHANT_RESTART:
            s->position = 0; s->completed = 0; s->playing = true; s->failed = false;
            break;
    }
}
bool chant_model_advance(chant_model_t *s, uint32_t count, uint32_t total) {
    if (!s->playing || s->failed || total == 0) return false;
    if (count >= total - s->position) {
        s->position = 0;
        if (s->sequence) s->track = (s->track + 1) % CHANT_TRACK_COUNT;
        if (s->completed < UINT32_MAX) s->completed++;
        return true;
    }
    s->position += count;
    return false;
}
