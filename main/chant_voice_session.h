#pragma once
#include "esp_http_client.h"
#include <stdbool.h>
/* One transaction, sequential ownership: audio worker, then joined downloader. */
typedef struct {
    esp_http_client_handle_t handle;
    bool server_close;
    unsigned requests, connections;
} chant_voice_session_t;
esp_http_client_handle_t chant_voice_session_prepare(chant_voice_session_t *session,
    const char *url, bool upload, const char *accept, const char *request_id,
    const char *key);
/* Only a fully consumed successful response can keep its connection. */
void chant_voice_session_finish(chant_voice_session_t *session, bool success);
void chant_voice_session_destroy(chant_voice_session_t *session);
