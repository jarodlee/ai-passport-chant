#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CHANT_VOICE_HEADER_BYTES 12
#define CHANT_VOICE_STREAM_BYTES (24 * 1024)
#define CHANT_VOICE_PREBUFFER_BYTES (16 * 1024)

/* PVA1: 16 kHz mono IMA ADPCM, zero predictor/index, high nibble first.
 * Header: magic[4], sample rate LE32, sample count LE32. */
bool chant_voice_stream_header(const uint8_t header[CHANT_VOICE_HEADER_BYTES],
                              int64_t content_bytes, uint32_t *samples);
