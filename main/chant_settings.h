#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Small persisted preferences kept in their own NVS namespace. */
bool chant_settings_init(void);
uint8_t chant_settings_voice_volume(void);
bool chant_settings_set_voice_volume(uint8_t value);
