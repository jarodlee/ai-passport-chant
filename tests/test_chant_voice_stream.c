#include "chant_voice_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++) p[i] = n >> (i * 8);
}
int main(void) {
    uint8_t h[12] = {'P','V','A','1'};
    uint32_t samples;
    put32(h + 4, 16000);
    const uint32_t counts[] = {1, 2, 511, 512, 16001, 16000 * 90};
    for (unsigned i = 0; i < sizeof(counts)/sizeof(counts[0]); i++) {
        put32(h + 8, counts[i]);
        int64_t length = 12 + (counts[i] + 1) / 2;
        assert(chant_voice_stream_header(h, length, &samples));
        assert(samples == counts[i]);
        assert(!chant_voice_stream_header(h, length - 1, &samples));
        assert(samples == 0);
        assert(!chant_voice_stream_header(h, length + 1, &samples));
    }
    put32(h + 8, 0);
    assert(!chant_voice_stream_header(h, 12, &samples));
    put32(h + 8, 16000 * 90 + 1);
    assert(!chant_voice_stream_header(h, 12 + (16000 * 90 + 2)/2, &samples));
    put32(h + 8, 16000);
    put32(h + 4, 8000);
    assert(!chant_voice_stream_header(h, 8012, &samples));
    put32(h + 4, 16000);
    h[0] = 'X';
    assert(!chant_voice_stream_header(h, 8012, &samples));
    memset(h, 0, sizeof(h)); /* old PCM cannot be played as compressed speech */
    assert(!chant_voice_stream_header(h, 8012, &samples));
    assert(!chant_voice_stream_header(NULL, 12, &samples));
    assert(!chant_voice_stream_header(h, 12, NULL));
    puts("Voice compressed stream protocol: PASS");
}
