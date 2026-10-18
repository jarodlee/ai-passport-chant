/* One worker owns Wi-Fi settings, NVS and HTTPS. Event/HTTP callbacks enqueue only.
 * Provisioning is explicit, temporarily open, AP-interface-only, and expires in 10 min.
 * Music/audio and LVGL are never accessed by the network service. */
#include "chant_network.h"
#if __has_include("chant_wifi_local.h")
#include "chant_wifi_local.h"
#else
#define CHANT_LOCAL_PROFILES { {"",""} }
#define CHANT_LOCAL_SETUP_ID 0u
#endif
#include "chant_voice.h"
#include "chant_services_model.h"
#include "chant_wifi_profiles.h"
#include "chant_storage.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

static const char *TAG = "chant_net";
static QueueHandle_t events;
static SemaphoreHandle_t state_lock;
static chant_network_state_t view;
static httpd_handle_t server;
static bool wifi_ready, connected, pending_save, have_credentials, sntp_started;
static unsigned retries, profile_failures;
static bool manual_attempt;
static chant_wifi_profiles_t profiles;
static const chant_wifi_profile_t local_profiles[]=CHANT_LOCAL_PROFILES;
static int64_t ap_deadline, ap_close_at, retry_at, weather_at;
static char saved_ssid[33], saved_password[64];
static void select_profile(void){
    if(!profiles.count){have_credentials=false;return;}
    memcpy(saved_ssid,profiles.entries[profiles.active].ssid,sizeof(saved_ssid));
    memcpy(saved_password,profiles.entries[profiles.active].password,sizeof(saved_password));
    have_credentials=true;
}


static esp_netif_t *sta, *ap;
/* Coalesce IP/disconnect state instead of relying on a lossy event queue. */
static portMUX_TYPE event_lock = portMUX_INITIALIZER_UNLOCKED;
static bool link_changed, link_up;
static esp_ip4_addr_t link_ip;

typedef struct { int command; char ssid[33], password[64]; unsigned profile; } request_t;
enum { REQUEST_CREDENTIALS = 100 };

static void publish_status(chant_net_status_t status) {
    xSemaphoreTake(state_lock, portMAX_DELAY);
    view.status = status;
    if (status!=NET_CONNECTED) { view.ip[0]=0; view.ssid[0]=0; }
    xSemaphoreGive(state_lock);
}
static void publish_profiles(void) {
    xSemaphoreTake(state_lock, portMAX_DELAY);
    view.profile_count=(uint8_t)profiles.count; view.active_profile=(uint8_t)profiles.active;
    for (unsigned i=0;i<profiles.count && i<CHANT_WIFI_MAX_PROFILES;i++)
        snprintf(view.profile_ssid[i], sizeof(view.profile_ssid[i]), "%s", profiles.entries[i].ssid);
    xSemaphoreGive(state_lock);
}
void chant_network_snapshot(chant_network_state_t *state) {
    if (!state_lock) { *state = (chant_network_state_t){.status=NET_FAILED}; return; }
    xSemaphoreTake(state_lock, portMAX_DELAY); *state = view; xSemaphoreGive(state_lock);
}
bool chant_network_command(chant_net_command_t command) {
    request_t request = {.command=command};
    return events && xQueueSend(events, &request, 0) == pdTRUE;
}
bool chant_network_connect_profile(uint8_t index) {
    request_t request = {.command=NET_CONNECT_PROFILE, .profile=index};
    return events && xQueueSend(events, &request, 0) == pdTRUE;
}
static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    bool changed = false, up = false;
    esp_ip4_addr_t ip = {0};
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) changed = true;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        changed = up = true; ip = ((ip_event_got_ip_t *)data)->ip_info.ip;
    }
    if (changed) {
        portENTER_CRITICAL(&event_lock);
        link_changed = true; link_up = up; link_ip = ip;
        portEXIT_CRITICAL(&event_lock);
    }
}
static uint32_t ipv4_of(const struct sockaddr_storage *ss) {
    const uint8_t *p = (const uint8_t *)ss;
    if (ss->ss_family == AF_INET) {
        uint32_t v; memcpy(&v, p + 4, 4); return ntohl(v);
    }
#ifdef LWIP_IPV6
    if (ss->ss_family == AF_INET6) {
        const uint8_t *a = p + 8;
        for (int i = 0; i < 10; i++) if (a[i]) return 0;
        if (a[10] != 0xFF || a[11] != 0xFF) return 0; /* only IPv4-mapped */
        uint32_t v; memcpy(&v, a + 12, 4); return ntohl(v);
    }
#endif
    return 0;
}
static bool from_ap(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    struct sockaddr_storage local = {0}, remote = {0};
    socklen_t ll = sizeof(local), rl = sizeof(remote);
    errno = 0; bool have_local = getsockname(fd, (struct sockaddr *)&local, &ll) == 0; int le = errno;
    errno = 0; bool have_remote = getpeername(fd, (struct sockaddr *)&remote, &rl) == 0; int re = errno;
    uint32_t l4 = have_local ? ipv4_of(&local) : 0;
    uint32_t r4 = have_remote ? ipv4_of(&remote) : 0;
    /* The phone must be an AP client (192.168.4.0/24). The Host header is a
       fallback for portal browsers whose accepted socket cannot be inspected;
       the provisioning AP is open and single-client, so this is not a stronger
       boundary than the AP itself. */
    bool ap_client = (r4 & 0xFFFFFF00u) == 0xC0A80400u;
    bool ap_local = l4 == 0xC0A80401u;
    char host[32] = {0};
    bool host_ok = httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK &&
                   strncmp(host, "192.168.4.1", 11) == 0 && (host[11] == 0 || host[11] == ':');
    if (!ap_client && !ap_local && !host_ok) {
        ESP_LOGW(TAG, "Portal rejected fd=%d le=%d re=%d fam=%d/%d l4=%08lx r4=%08lx host=%s",
                 fd, le, re, (int)local.ss_family, (int)remote.ss_family,
                 (unsigned long)l4, (unsigned long)r4, host);
    }
    return ap_client || ap_local || host_ok;
}
/* Chinese is entity-encoded (and JS uses \u escapes) so the page also renders
   correctly in phone portal browsers that ignore the UTF-8 charset. */
static const char portal[] =
"<!doctype html><html lang=zh-CN><head><meta charset=utf-8>"
"<meta http-equiv=Content-Type content='text/html; charset=utf-8'>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>&#x5B89;&#x5FC3;&#x542C; &#xB7; Wi-Fi&#x914D;&#x7F51;</title>"
"<style>body{font:18px system-ui;background:#162a26;color:#efe6d2;max-width:480px;margin:40px auto;padding:20px}input,button{box-sizing:border-box;width:100%;padding:14px;margin:10px 0;border-radius:10px}button{background:#dec28b;border:0}small{line-height:1.6}"
"</style></head><body><h1>&#x8FDE;&#x63A5; Wi-Fi</h1>"
"<p>&#x586B;&#x5199;&#x529E;&#x516C;&#x5BA4;&#x6216;&#x5BB6;&#x91CC;&#x7684; 2.4 GHz &#x7F51;&#x7EDC;&#x3002;"
"</p><form id=f><label>Wi-Fi&#x540D;&#x79F0;<input id=s required maxlength=32 autocomplete=off></label>"
"<label>&#x5BC6;&#x7801;<input id=p type=password minlength=8 maxlength=63 autocomplete=new-password></label>"
"<small>&#x5F00;&#x653E;&#x7F51;&#x7EDC;&#x8BF7;&#x7559;&#x7A7A;&#x3002;&#x5BC6;&#x7801;&#x4E0D;&#x663E;&#x793A;&#x3001;&#x4E0D;&#x4E0A;&#x4F20;&#x5230;&#x4E91;&#x7AEF;&#xFF1B;&#x8FDE;&#x63A5;&#x6210;&#x529F;&#x540E;&#x4FDD;&#x5B58;&#x5728;&#x8BBE;&#x5907;&#x3002;"
"</small><button>&#x8FDE;&#x63A5;&#x5E76;&#x4FDD;&#x5B58;</button></form><p id=r role=status></p>"
"<p>&#x63D0;&#x4EA4;&#x540E;&#x8BF7;&#x770B;&#x8BBE;&#x5907;&#x5C4F;&#x5E55;&#x786E;&#x8BA4;&#x8FDE;&#x63A5;&#x7ED3;&#x679C;&#xFF0C;&#x624B;&#x673A;&#x53EF;&#x5207;&#x56DE;&#x539F;&#x7F51;&#x7EDC;&#x3002;"
"</p>"
"<script>f.onsubmit=async e=>{e.preventDefault();let b=f.querySelector('button');b.disabled=true;r.textContent='\\u6B63\\u5728\\u63D0\\u4EA4\\u2026';try{let x=await fetch('/configure',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s.value,password:p.value})});let t=await x.text();r.textContent=x.ok?t:x.status===400?'\\u63D0\\u4EA4\\u5931\\u8D25\\uFF0C\\u8BF7\\u68C0\\u67E5\\u7F51\\u7EDC\\u540D\\u53CA\\u5BC6\\u7801':x.status===503?'\\u8BBE\\u5907\\u5FD9\\uFF0C\\u8BF7\\u7A0D\\u540E\\u91CD\\u8BD5':'\\u63D0\\u4EA4\\u5931\\u8D25\\uFF0C\\u8BF7\\u7A0D\\u540E\\u91CD\\u8BD5'}catch(e){r.textContent='\\u63D0\\u4EA4\\u5931\\u8D25\\uFF0C\\u8BF7\\u4FDD\\u6301\\u8FDE\\u63A5\\u8BBE\\u5907\\u70ED\\u70B9\\u540E\\u91CD\\u8BD5'}b.disabled=false}"
"</script></body></html>";
static esp_err_t send_notice(httpd_req_t *req, const char *status, const char *html_message) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    char page[512];
    snprintf(page, sizeof(page),
        "<!doctype html><html lang=zh-CN><head><meta charset=utf-8></head>"
        "<body style=\"font:18px system-ui;background:#162a26;color:#efe6d2;padding:24px\">"
        "<p>%s</p><p><a style=\"color:#dec28b\" href=\"/\">%s</a></p></body></html>",
        html_message, "&#x8FD4;&#x56DE;&#x914D;&#x7F51;&#x9875;");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t send_text(httpd_req_t *req, const char *status, const char *message) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, message);
}
static esp_err_t home(httpd_req_t *req) {
    if (!from_ap(req)) return send_notice(req, "403 Forbidden", "&#x8BF7;&#x8FDE;&#x63A5;&#x8BBE;&#x5907;&#x70ED;&#x70B9;&#x540E;&#x6253;&#x5F00;&#x914D;&#x7F51;&#x9875;");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, portal, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t configure(httpd_req_t *req) {
    if (!from_ap(req)) return send_notice(req, "403 Forbidden", "&#x8BF7;&#x8FDE;&#x63A5;&#x8BBE;&#x5907;&#x70ED;&#x70B9;&#x540E;&#x6253;&#x5F00;&#x914D;&#x7F51;&#x9875;");
    if (req->content_len == 0 || req->content_len > 512) return send_text(req, "400 Bad Request", "提交内容过长或为空");
    char body[513]; size_t used = 0;
    int64_t deadline=esp_timer_get_time()+INT64_C(5000000);
    while (used < req->content_len) {
        if (esp_timer_get_time()>deadline) return send_text(req,"408 Request Timeout","提交超时，请重试");
        int n = httpd_req_recv(req, body + used, req->content_len - used);
        if (n <= 0) return send_text(req, "400 Bad Request", "接收失败，请重试");
        used += n;
    }
    body[used] = 0;
    if (memchr(body, 0, used) || strstr(body, "\\u0000")) return send_text(req, "400 Bad Request", "输入含无效字符");
    const char *end = NULL;
    cJSON *json = cJSON_ParseWithLengthOpts(body, used + 1, &end, true);
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
    bool good = cJSON_IsString(ssid) && cJSON_IsString(password) &&
                chant_wifi_credentials_valid(ssid->valuestring, password->valuestring);
    request_t request = {.command=REQUEST_CREDENTIALS};
    if (good) {
        snprintf(request.ssid, sizeof(request.ssid), "%s", ssid->valuestring);
        snprintf(request.password, sizeof(request.password), "%s", password->valuestring);
    }
    cJSON_Delete(json);
    if (!good) return send_text(req, "400 Bad Request", "网络名需为1至32字节；密码留空或填写8至63位英文字符");
    if (xQueueSend(events, &request, 0) != pdTRUE) return send_text(req,"503 Service Unavailable","设备忙，请稍后重试");
    return send_text(req, "200 OK", "已提交，请在设备屏幕查看连接结果。连接成功后热点会关闭。若密码错误，可重新提交。");
}
static void end_provision(void) {
    if (server) { httpd_stop(server); server = NULL; }
    if (wifi_ready) esp_wifi_set_mode(WIFI_MODE_STA);
    ap_deadline = ap_close_at = 0;
    xSemaphoreTake(state_lock, portMAX_DELAY); view.provisioning = false; xSemaphoreGive(state_lock);
}
static esp_err_t begin_provision(void) {
    end_provision();
    wifi_config_t config = {0};
    uint32_t random = esp_random();
    snprintf((char *)config.ap.ssid, sizeof(config.ap.ssid), "Passport-%04X", (unsigned)(random & 0xFFFF));
    config.ap.ssid_len = strlen((char *)config.ap.ssid);
    config.ap.max_connection = 1; config.ap.channel = 1;
    config.ap.authmode = WIFI_AUTH_OPEN;
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &config);
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.stack_size = 4096; http.max_open_sockets = 2;
    http.recv_wait_timeout = 5; http.send_wait_timeout = 5;
    if (err == ESP_OK) err = httpd_start(&server, &http);
    httpd_uri_t get = {.uri="/", .method=HTTP_GET, .handler=home};
    httpd_uri_t post = {.uri="/configure", .method=HTTP_POST, .handler=configure};
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &get);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &post);
    if (err != ESP_OK) { end_provision(); return err; }
    xSemaphoreTake(state_lock, portMAX_DELAY);
    snprintf(view.ap_name, sizeof(view.ap_name), "%.23s", config.ap.ssid);
    snprintf(view.ap_password, sizeof(view.ap_password), "%.12s", config.ap.password);
    view.provisioning = true;
    xSemaphoreGive(state_lock);
    ap_deadline = esp_timer_get_time() + INT64_C(600000000);
    return ESP_OK;
}
typedef struct { char ssid[33], password[64]; } stored_credentials_t;
static esp_err_t save_credentials(bool clear) {
    esp_err_t err=chant_storage_begin();
    if (err != ESP_OK) return err;
    nvs_handle_t handle;
    err=nvs_open("chant_wifi",NVS_READWRITE,&handle);
    if (err == ESP_OK) {
        if (clear) {
            err=nvs_erase_key(handle,"profiles");
            if (err==ESP_ERR_NVS_NOT_FOUND) err=ESP_OK;
            if (err==ESP_OK) {
                esp_err_t legacy=nvs_erase_key(handle,"network");
                if (legacy!=ESP_OK && legacy!=ESP_ERR_NVS_NOT_FOUND) err=legacy;
            }
        }
        else {
            /* The whole validated list and preferred entry commit together. */
            err=chant_wifi_profiles_valid(&profiles)?nvs_set_blob(handle,"profiles",&profiles,sizeof(profiles)):ESP_ERR_INVALID_STATE;
        }
        /* Preserve the one-time setup marker when forgetting the network. */
        if (err==ESP_OK && CHANT_LOCAL_SETUP_ID) err=nvs_set_u32(handle,"setup_id",CHANT_LOCAL_SETUP_ID);
        if (err==ESP_OK) err=nvs_commit(handle);
        nvs_close(handle);
    }
    chant_storage_end(); return err;
}
static esp_err_t connect_saved(void) {
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, saved_ssid, strlen(saved_ssid));
    memcpy(config.sta.password, saved_password, strlen(saved_password));
    config.sta.threshold.authmode = saved_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err == ESP_OK) err = esp_wifi_connect();
    return err;
}
static esp_err_t initialize(void) {
    setenv("TZ", "CST-8", 1); tzset();
    esp_err_t err = nvs_flash_init(); /* Never erase on NVS errors. */
    if (err != ESP_OK) return err;
    err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK) return err;
    esp_netif_config_t sta_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    esp_netif_config_t ap_cfg = ESP_NETIF_DEFAULT_WIFI_AP();
    sta = esp_netif_new(&sta_cfg); ap = esp_netif_new(&ap_cfg);
    if (!sta || !ap) return ESP_ERR_NO_MEM;
    err = esp_netif_attach_wifi_station(sta);
    if (err == ESP_OK) err = esp_wifi_set_default_wifi_sta_handlers();
    if (err == ESP_OK) err = esp_netif_attach_wifi_ap(ap);
    if (err == ESP_OK) err = esp_wifi_set_default_wifi_ap_handlers();
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    if (err == ESP_OK) err = esp_wifi_init(&config);
    if (err != ESP_OK) return err;
    wifi_ready = true;
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL);
    if (err == ESP_OK) err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL);
    if (err == ESP_OK) err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) return err;
    chant_wifi_profiles_init(&profiles);
    nvs_handle_t handle; uint32_t setup_id=0;
    if (nvs_open("chant_wifi", NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u32(handle,"setup_id",&setup_id);
        size_t size=sizeof(profiles);
        esp_err_t result=nvs_get_blob(handle,"profiles",&profiles,&size);
        if(result!=ESP_OK || size!=sizeof(profiles) || !chant_wifi_profiles_valid(&profiles)){
            chant_wifi_profiles_init(&profiles);
            /* Migrate a successful connection from the older single-network format. */
            stored_credentials_t data={0};size=sizeof(data);
            if(nvs_get_blob(handle,"network",&data,&size)==ESP_OK && size==sizeof(data) &&
               memchr(data.ssid,0,sizeof(data.ssid)) && memchr(data.password,0,sizeof(data.password)) &&
               chant_wifi_profiles_put(&profiles,data.ssid,data.password)>=0) pending_save=true;
            memset(&data,0,sizeof(data));
        }
        nvs_close(handle);
    }
    if(CHANT_LOCAL_SETUP_ID && setup_id!=CHANT_LOCAL_SETUP_ID){
        for(unsigned i=0;i<sizeof(local_profiles)/sizeof(local_profiles[0]);i++)
            if(chant_wifi_profiles_put(&profiles,local_profiles[i].ssid,local_profiles[i].password)>=0)pending_save=true;
    }
    select_profile();
    publish_profiles();
    ESP_LOGI(TAG,"Saved Wi-Fi profiles: %u",(unsigned)profiles.count);
    if (have_credentials) {
        err = connect_saved(); publish_status(err == ESP_OK ? NET_CONNECTING : NET_BAD_WIFI);
    } else publish_status(NET_OFFLINE);
    return ESP_OK;
}
static void refresh_weather(void) {
    bool success = false;
    /* SNTP supplies a real wall clock for certificate validity; never bypass TLS. */
    if (time(NULL) < 1700000000) goto done;
    xSemaphoreTake(state_lock, portMAX_DELAY); view.weather_refreshing=true; xSemaphoreGive(state_lock);
    esp_http_client_config_t config = {
        .url="https://api.open-meteo.com/v1/forecast?latitude=30.54&longitude=117.06&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Asia%2FShanghai",
        .crt_bundle_attach=esp_crt_bundle_attach, .timeout_ms=8000,
        .buffer_size=1024, .buffer_size_tx=512, .disable_auto_redirect=true
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *body = NULL;
    if (!client) goto done;
    if (esp_http_client_open(client, 0) != ESP_OK) goto cleanup;
    int64_t length = esp_http_client_fetch_headers(client);
    if (esp_http_client_get_status_code(client) != 200 || length > 4096) goto cleanup;
    body = malloc(4097);
    if (!body) goto cleanup;
    int used = 0;
    int64_t deadline = esp_timer_get_time() + INT64_C(12000000);
    while (used < 4096 && esp_timer_get_time() < deadline) {
        int n = esp_http_client_read(client, body + used, 4096 - used);
        if (n < 0) goto cleanup;
        if (n == 0) break;
        used += n;
    }
    if (!esp_http_client_is_complete_data_received(client) || used == 4096) goto cleanup;
    body[used] = 0;
    cJSON *root = cJSON_Parse(body);
    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    cJSON *humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
    cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
    cJSON *stamp = cJSON_GetObjectItemCaseSensitive(current, "time");
    if (cJSON_IsNumber(temp) && cJSON_IsNumber(humidity) && cJSON_IsNumber(code) && cJSON_IsString(stamp) &&
        strlen(stamp->valuestring) == 16 && chant_weather_values_valid(temp->valuedouble, humidity->valuedouble, code->valuedouble)) {
        /* Only publish numeric ASCII date/time, never unbounded network text. */
        const char *s = stamp->valuestring;
        bool stamp_ok = s[4]=='-' && s[7]=='-' && s[10]=='T' && s[13]==':';
        for (int i=0; i<16; i++) if (i!=4 && i!=7 && i!=10 && i!=13 && (s[i]<'0' || s[i]>'9')) stamp_ok = false;
        struct tm observation = {0};
        time_t observed = 0;
        int year, month, day, hour, minute;
        if (sscanf(s, "%4d-%2d-%2dT%2d:%2d", &year, &month, &day, &hour, &minute) != 5) stamp_ok=false;
        else if (year<2020 || year>2100 || month<1 || month>12 || day<1 || day>31 || hour>23 || minute>59) stamp_ok=false;
        else {
            observation.tm_year=year-1900; observation.tm_mon=month-1; observation.tm_mday=day;
            observation.tm_hour=hour; observation.tm_min=minute; observation.tm_isdst=-1;
            observed=mktime(&observation);
            time_t now=time(NULL);
            if (observed>now+120 || now-observed>1800) stamp_ok=false;
        }
        if (stamp_ok) {
            xSemaphoreTake(state_lock, portMAX_DELAY);
            view.temperature=temp->valuedouble; view.humidity=(unsigned)humidity->valuedouble;
            view.weather_code=code->valueint;
            snprintf(view.weather_time, sizeof(view.weather_time), "%s", s);
            view.weather_valid=true;
            view.weather_updated_us=esp_timer_get_time(); view.weather_fetched_us=view.weather_updated_us;
            view.weather_observed_at=(int64_t)observed;
            xSemaphoreGive(state_lock); success=true;
        }
    }
    cJSON_Delete(root);
cleanup:
    free(body); esp_http_client_close(client); esp_http_client_cleanup(client);
done:
    xSemaphoreTake(state_lock, portMAX_DELAY); view.weather_error=!success; view.weather_refreshing=false; xSemaphoreGive(state_lock);
    weather_at = esp_timer_get_time() + (success ? INT64_C(900000000) : INT64_C(60000000));
    ESP_LOGI(TAG, "Weather %s: %.1f C %u%% code=%d; free heap=%u", success ? "updated" : "unavailable", (double)view.temperature, view.humidity, view.weather_code, (unsigned)esp_get_free_heap_size());
}
static void scan_insert(chant_scan_ap_t *list, unsigned *count, const wifi_ap_record_t *record) {
    if (record->ssid[0] == 0) return; /* hidden network */
    for (unsigned i = 0; i < *count; i++) {
        if (!strncmp((const char *)record->ssid, list[i].ssid, 32)) {
            if (record->rssi > list[i].rssi) {
                list[i].rssi = record->rssi; list[i].channel = record->primary;
                list[i].secure = record->authmode != WIFI_AUTH_OPEN;
            }
            return;
        }
    }
    chant_scan_ap_t ap = {0};
    snprintf(ap.ssid, sizeof(ap.ssid), "%.32s", (const char *)record->ssid);
    ap.rssi = record->rssi; ap.channel = record->primary;
    ap.secure = record->authmode != WIFI_AUTH_OPEN;
    unsigned pos;
    if (*count < CHANT_SCAN_MAX) pos = (*count)++;
    else if (ap.rssi > list[CHANT_SCAN_MAX - 1].rssi) pos = CHANT_SCAN_MAX - 1;
    else return;
    while (pos > 0 && list[pos - 1].rssi < ap.rssi) { list[pos] = list[pos - 1]; pos--; }
    list[pos] = ap;
}
static void scan_networks(void) {
    xSemaphoreTake(state_lock, portMAX_DELAY);
    view.scanning = true; view.scan_error = false;
    xSemaphoreGive(state_lock);
    chant_scan_ap_t found[CHANT_SCAN_MAX] = {0};
    unsigned count = 0;
    wifi_scan_config_t config = {0};
    esp_err_t err = esp_wifi_scan_start(&config, true);
    if (err == ESP_OK) {
        uint16_t number = 0;
        err = esp_wifi_scan_get_ap_num(&number);
        if (err == ESP_OK) {
            if (number > 32) number = 32;
            wifi_ap_record_t *records = calloc(number ? number : 1, sizeof(*records));
            if (!records) { esp_wifi_clear_ap_list(); err = ESP_ERR_NO_MEM; }
            else {
                uint16_t got = number;
                err = esp_wifi_scan_get_ap_records(&got, records);
                if (err == ESP_OK) {
                    for (uint16_t i = 0; i < got; i++) scan_insert(found, &count, &records[i]);
                } else esp_wifi_clear_ap_list();
                free(records);
            }
        }
    }
    xSemaphoreTake(state_lock, portMAX_DELAY);
    view.scanning = false; view.scan_error = err != ESP_OK;
    view.scan_count = (uint8_t)count;
    memcpy(view.scan, found, count * sizeof(found[0]));
    xSemaphoreGive(state_lock);
    ESP_LOGI(TAG, "Wi-Fi scan: %u networks (%s)", count, esp_err_to_name(err));
}
static void network_worker(void *arg) {
    (void)arg;
    esp_err_t init_error = initialize();
    if (init_error != ESP_OK) {
        ESP_LOGE(TAG, "Network init: %s", esp_err_to_name(init_error));
        esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,on_wifi);
        esp_event_handler_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,on_wifi);
        if (wifi_ready) { esp_wifi_stop(); esp_wifi_deinit(); wifi_ready=false; }
        if (sta) { esp_netif_destroy_default_wifi(sta); sta=NULL; }
        if (ap) { esp_netif_destroy_default_wifi(ap); ap=NULL; }
        publish_status(NET_FAILED); vTaskDelete(NULL); return;
    }
    for (;;) {
        int64_t now = esp_timer_get_time();
        bool changed, up; esp_ip4_addr_t ip;
        portENTER_CRITICAL(&event_lock);
        changed=link_changed; up=link_up; ip=link_ip; link_changed=false;
        portEXIT_CRITICAL(&event_lock);
        if (changed) {
            connected=up;
            if (up) {
                retries=profile_failures=0; retry_at=0; publish_status(NET_CONNECTED);
                xSemaphoreTake(state_lock, portMAX_DELAY);
                snprintf(view.ip, sizeof(view.ip), IPSTR, IP2STR(&ip));
                xSemaphoreGive(state_lock);
                wifi_ap_record_t access_point={0};
                bool have_ap=esp_wifi_sta_get_ap_info(&access_point)==ESP_OK;
                bool matching=have_ap &&
                    strnlen((char *)access_point.ssid,32)==strlen(saved_ssid) &&
                    memcmp(access_point.ssid,saved_ssid,strlen(saved_ssid))==0;
                xSemaphoreTake(state_lock, portMAX_DELAY);
                snprintf(view.ssid, sizeof(view.ssid), "%s", have_ap ? (char *)access_point.ssid : saved_ssid);
                xSemaphoreGive(state_lock);
                ESP_LOGI(TAG, "Connected to %s", view.ssid);
                if (pending_save && !matching) {
                    connected=false; publish_status(NET_CONNECTING);
                    retry_at=now+INT64_C(3000000); continue;
                }
                if (pending_save && matching) {
                    int selected=manual_attempt?chant_wifi_profiles_put(&profiles,saved_ssid,saved_password):(int)profiles.active;
                    if(selected>=0)profiles.active=(unsigned)selected;
                    esp_err_t error=selected>=0?save_credentials(false):ESP_ERR_NO_MEM;
                    pending_save=false;
                    xSemaphoreTake(state_lock, portMAX_DELAY); view.save_error=error!=ESP_OK; xSemaphoreGive(state_lock);
                    publish_profiles();
                    ap_close_at=now+INT64_C(15000000);
                }
                manual_attempt=false;
                if (!sntp_started) {
                    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                    esp_sntp_setservername(0, "ntp.aliyun.com");
                    esp_sntp_setservername(1, "pool.ntp.org");
                    esp_sntp_init(); sntp_started=true;
                }
                weather_at=now+INT64_C(5000000);
            } else if (have_credentials) {
                retries++;profile_failures++;
                if(!manual_attempt && profiles.count>1 && profile_failures>=2){
                    chant_wifi_profiles_next(&profiles);select_profile();profile_failures=0;pending_save=true;
                    publish_profiles();
                    ESP_LOGI(TAG,"Trying saved Wi-Fi profile %u/%u",(unsigned)profiles.active+1,(unsigned)profiles.count);
                }
                unsigned fast_attempts=manual_attempt?5:(profiles.count?profiles.count*2:5);
                publish_status(retries < fast_attempts ? NET_CONNECTING : NET_BAD_WIFI);
                retry_at=now+(retries < fast_attempts ? INT64_C(3000000) : INT64_C(30000000));
            } else publish_status(NET_OFFLINE);
        }
        request_t request;
        if (xQueueReceive(events, &request, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (request.command == NET_PROVISION) {
                if (begin_provision() != ESP_OK) publish_status(NET_FAILED);
            } else if (request.command == NET_CANCEL_PROVISION) end_provision();
            else if (request.command == NET_REFRESH_WEATHER) { weather_at=0; ESP_LOGI(TAG, "Weather refresh requested"); }
            else if (request.command == NET_SCAN) scan_networks();
            else if (request.command == NET_CONNECT_PROFILE) {
                if (request.profile < profiles.count) {
                    esp_wifi_disconnect(); connected=false; retries=profile_failures=0; retry_at=0;
                    profiles.active=request.profile; select_profile();
                    ESP_LOGI(TAG,"Selecting saved profile %u/%u",(unsigned)profiles.active+1,(unsigned)profiles.count);
                    manual_attempt=true; pending_save=false; have_credentials=true;
                    esp_err_t error=save_credentials(false);
                    if (error != ESP_OK) ESP_LOGW(TAG,"Selected profile not persisted: %s", esp_err_to_name(error));
                    publish_profiles();
                    publish_status(connect_saved()==ESP_OK?NET_CONNECTING:NET_BAD_WIFI);
                }
            } else if (request.command == NET_FORGET) {
                esp_err_t error=save_credentials(true);
                xSemaphoreTake(state_lock, portMAX_DELAY); view.save_error=error!=ESP_OK; xSemaphoreGive(state_lock);
                if (error == ESP_OK) {
                    have_credentials=pending_save=manual_attempt=false; connected=false; retry_at=0;
                    chant_wifi_profiles_init(&profiles);
                    memset(saved_ssid,0,sizeof(saved_ssid)); memset(saved_password,0,sizeof(saved_password));
                    publish_profiles();
                    esp_wifi_disconnect(); end_provision(); publish_status(NET_OFFLINE);
                }
            } else if (request.command == REQUEST_CREDENTIALS) {
                /* Clear any old pending disconnect before reconfiguration. */
                esp_wifi_disconnect(); connected=false; retries=profile_failures=0; retry_at=0;
                snprintf(saved_ssid, sizeof(saved_ssid), "%s", request.ssid);
                snprintf(saved_password, sizeof(saved_password), "%s", request.password);
                have_credentials=pending_save=manual_attempt=true;
                ap_close_at=0;
                publish_status(connect_saved() == ESP_OK ? NET_CONNECTING : NET_BAD_WIFI);
                memset(&request, 0, sizeof(request));
            }
        }
        now=esp_timer_get_time();
        if (retry_at && now>=retry_at && !connected && have_credentials) {
            retry_at=0;
            if (connect_saved() != ESP_OK) retry_at=now+INT64_C(30000000);
        }
        if ((ap_deadline && now>=ap_deadline) || (ap_close_at && now>=ap_close_at)) end_provision();
        if (connected && now>=weather_at && !ap_deadline && !chant_voice_busy()) refresh_weather();
    }
}
bool chant_network_start(void) {
    state_lock=xSemaphoreCreateMutex(); events=xQueueCreate(4,sizeof(request_t));
    if (!state_lock || !events) {
        if (state_lock) publish_status(NET_FAILED);
        return false;
    }
    view.status=NET_STARTING;
    if (xTaskCreate(network_worker,"chant_network",8192,NULL,3,NULL) != pdPASS) {
        publish_status(NET_FAILED); return false;
    }
    return true;
}
