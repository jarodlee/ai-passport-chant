#pragma once
#include <stdatomic.h>
#include <stdbool.h>

typedef struct { atomic_uint flags; } chant_voice_control_t;
void chant_voice_control_press(chant_voice_control_t *state, bool down);
void chant_voice_control_cancel(chant_voice_control_t *state);
bool chant_voice_control_begin(chant_voice_control_t *state);
void chant_voice_control_finish(chant_voice_control_t *state);
bool chant_voice_control_requested(const chant_voice_control_t *state);
bool chant_voice_control_busy(const chant_voice_control_t *state);
bool chant_voice_control_held(const chant_voice_control_t *state);
bool chant_voice_control_cancelled(const chant_voice_control_t *state);
