#pragma once
#include <stdbool.h>
#include <stdint.h>
/* The Mac renders each reply as a 1bpp bitmap so the device needs no full CJK font. */
#define CHANT_REPLY_WIDTH 224
#define CHANT_REPLY_HEIGHT 168
#define CHANT_REPLY_BYTES ((CHANT_REPLY_WIDTH / 8) * CHANT_REPLY_HEIGHT)
/* Indexed LVGL images keep a 2-entry palette in front of the pixel rows. */
#define CHANT_REPLY_PALETTE_BYTES 8
void chant_voice_press(bool down);
bool chant_voice_requested(void);
bool chant_voice_busy(void);
void chant_voice_run(uint8_t volume);
const char *chant_voice_status(void);
/* Returns the last reply bitmap; serial changes when a new reply replaces it. */
bool chant_voice_reply(const uint8_t **pixels, unsigned *serial);
void chant_voice_clear_reply(void);
void chant_voice_cancel(void);
