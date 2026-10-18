#include "chant_settings.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NAMESPACE "chant_cfg"
#define KEY_VOICE "voice_vol"
#define VOICE_DEFAULT 80
#define VOICE_MIN 20
#define VOICE_MAX 100

static bool initialized, opened;
static nvs_handle_t handle;
static uint8_t cached = VOICE_DEFAULT;

bool chant_settings_init(void) {
    if (initialized) return true;
    initialized = true;
    /* Never erase NVS on errors: card identity and storage share the flash. */
    if (nvs_flash_init() != ESP_OK) return false;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;
    opened = true;
    uint8_t value;
    if (nvs_get_u8(handle, KEY_VOICE, &value) == ESP_OK && value >= VOICE_MIN && value <= VOICE_MAX)
        cached = value;
    return true;
}

uint8_t chant_settings_voice_volume(void) { return cached; }

bool chant_settings_set_voice_volume(uint8_t value) {
    if (value < VOICE_MIN) value = VOICE_MIN;
    if (value > VOICE_MAX) value = VOICE_MAX;
    cached = value;
    if (!opened) return false;
    if (nvs_set_u8(handle, KEY_VOICE, value) != ESP_OK) return false;
    return nvs_commit(handle) == ESP_OK;
}
