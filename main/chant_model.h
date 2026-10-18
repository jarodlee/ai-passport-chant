#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { CHANT_TOGGLE, CHANT_UP, CHANT_DOWN, CHANT_RESTART, CHANT_SELECT_0, CHANT_SELECT_1, CHANT_SELECT_2, CHANT_SELECT_3, CHANT_SELECT_4, CHANT_SEQUENCE, CHANT_VOICE_UP, CHANT_VOICE_DOWN, CHANT_VOICE_TEST } chant_command_t;
typedef struct {
    uint32_t position;
    uint32_t completed;
    uint8_t volume;
    uint8_t voice_volume;
    uint8_t track;
    bool sequence;
    bool playing;
    bool failed;
} chant_model_t;
void chant_model_init(chant_model_t *state);
void chant_model_command(chant_model_t *state, chant_command_t command);
bool chant_model_advance(chant_model_t *state, uint32_t samples, uint32_t total);
