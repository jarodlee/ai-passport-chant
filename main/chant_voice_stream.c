#include "chant_voice_stream.h"
#include <string.h>

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool chant_voice_stream_header(const uint8_t h[CHANT_VOICE_HEADER_BYTES],
                              int64_t bytes, uint32_t *samples) {
    if (!h || !samples) return false;
    *samples = 0;
    uint32_t count = le32(h + 8);
    if (memcmp(h, "PVA1", 4) || le32(h + 4) != 16000 ||
        !count || count > 16000 * 90 ||
        bytes != CHANT_VOICE_HEADER_BYTES + (int64_t)(count + 1) / 2)
        return false;
    *samples = count;
    return true;
}
