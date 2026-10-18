#include "chant_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#define REQUEST (1U<<0)
#define QUIET (1U<<1)
#define DONE (1U<<2)
static EventGroupHandle_t gate;
bool chant_storage_init(void) { gate=xEventGroupCreate(); return gate!=NULL; }
esp_err_t chant_storage_begin(void) {
    if (!gate) return ESP_ERR_INVALID_STATE;
    xEventGroupClearBits(gate,QUIET|DONE);
    xEventGroupSetBits(gate,REQUEST);
    if (xEventGroupWaitBits(gate,QUIET,pdFALSE,pdTRUE,pdMS_TO_TICKS(2000)) & QUIET) return ESP_OK;
    chant_storage_end(); return ESP_ERR_TIMEOUT;
}
void chant_storage_end(void) {
    xEventGroupClearBits(gate,REQUEST);
    xEventGroupSetBits(gate,DONE);
}
bool chant_storage_requested(void) { return gate && (xEventGroupGetBits(gate)&REQUEST)!=0; }
void chant_storage_quiet(bool success) { if (success) xEventGroupSetBits(gate,QUIET); }
void chant_storage_wait_done(void) { xEventGroupWaitBits(gate,DONE,pdFALSE,pdTRUE,portMAX_DELAY); }
