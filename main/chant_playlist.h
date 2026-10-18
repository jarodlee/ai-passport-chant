#pragma once
#include <stdint.h>
#define CHANT_TRACK_COUNT 5
#define CHANT_AUDIO_RATE 11025
/* ROM assets. Never allocate a whole recording in RAM. */
typedef struct { const char *title; const uint8_t *start, *end; uint32_t samples; } chant_track_t;
extern const chant_track_t chant_tracks[CHANT_TRACK_COUNT];
