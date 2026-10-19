#pragma once
#include <stdbool.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define HTTP_METHOD_GET 0
#define HTTP_METHOD_POST 1
#define HTTP_EVENT_ON_HEADER 1
#define HTTP_EVENT_ON_CONNECTED 2
typedef struct fake_http *esp_http_client_handle_t;
typedef struct {
    int event_id;
    void *user_data;
    char *header_key, *header_value;
} esp_http_client_event_t;
typedef struct {
    const char *url;
    void (*crt_bundle_attach)(void);
    int timeout_ms, buffer_size, buffer_size_tx;
    bool disable_auto_redirect, keep_alive_enable;
    esp_err_t (*event_handler)(esp_http_client_event_t *);
    void *user_data;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_set_url(esp_http_client_handle_t,const char *);
int esp_http_client_set_method(esp_http_client_handle_t,int);
int esp_http_client_set_timeout_ms(esp_http_client_handle_t,int);
int esp_http_client_set_header(esp_http_client_handle_t,const char *,const char *);
int esp_http_client_delete_header(esp_http_client_handle_t,const char *);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
int esp_http_client_close(esp_http_client_handle_t);
int esp_http_client_cleanup(esp_http_client_handle_t);
