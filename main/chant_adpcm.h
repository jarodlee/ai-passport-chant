#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { int predictor; int index; } chant_decoder_t;
void chant_decode(chant_decoder_t *state, const uint8_t *encoded,
                  size_t first_sample, size_t count, int16_t *pcm);

unsigned chant_encode_sample(int sample, int *predictor, int *index);
