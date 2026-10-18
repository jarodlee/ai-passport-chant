#pragma once
#include "esp_err.h"
#include <stdbool.h>
/* Audio owner briefly sleeps the codec before NVS writes, then resumes the
 * same playback position. Flash cache stalls must not drain an active I2S DMA. */
bool chant_storage_init(void);
esp_err_t chant_storage_begin(void);
void chant_storage_end(void);
bool chant_storage_requested(void);
void chant_storage_quiet(bool success);
void chant_storage_wait_done(void);
