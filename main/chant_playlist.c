#include "chant_playlist.h"
extern const uint8_t dabei_start[] asm("_binary_dabei_adpcm_start");
extern const uint8_t dabei_end[] asm("_binary_dabei_adpcm_end");
extern const uint8_t guanyin_start[] asm("_binary_guanyin_adpcm_start");
extern const uint8_t guanyin_end[] asm("_binary_guanyin_adpcm_end");
extern const uint8_t auspicious_start[] asm("_binary_auspicious_adpcm_start");
extern const uint8_t auspicious_end[] asm("_binary_auspicious_adpcm_end");
extern const uint8_t xinjing_start[] asm("_binary_xinjing_adpcm_start");
extern const uint8_t xinjing_end[] asm("_binary_xinjing_adpcm_end");
extern const uint8_t xinjinglong_start[] asm("_binary_xinjinglong_adpcm_start");
extern const uint8_t xinjinglong_end[] asm("_binary_xinjinglong_adpcm_end");
const chant_track_t chant_tracks[CHANT_TRACK_COUNT] = {
    {"大悲咒", dabei_start, dabei_end, 5622750u},
    {"观音菩萨圣号", guanyin_start, guanyin_end, 411552u},
    {"消灾吉祥神咒", auspicious_start, auspicious_end, 844992u},
    {"心经", xinjing_start, xinjing_end, 3371996u},
    {"心经长版", xinjinglong_start, xinjinglong_end, 3197250u},
};
