#include "chant_voice_session.h"
#include "esp_crt_bundle.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static esp_err_t response_event(esp_http_client_event_t *event)
{
    chant_voice_session_t *session=event->user_data;
    if(event->event_id==HTTP_EVENT_ON_CONNECTED)session->connections++;
    if(event->event_id==HTTP_EVENT_ON_HEADER && event->header_key && event->header_value &&
       !strcasecmp(event->header_key,"Connection") &&
       !strcasecmp(event->header_value,"close")) session->server_close=true;
    return ESP_OK;
}

esp_http_client_handle_t chant_voice_session_prepare(chant_voice_session_t *session,
    const char *url, bool upload, const char *accept, const char *request_id,
    const char *key)
{
    if(!session->handle){
        esp_http_client_config_t config={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,
            .timeout_ms=15000,.buffer_size=1024,.buffer_size_tx=1024,
            .disable_auto_redirect=true,.keep_alive_enable=true,
            .event_handler=response_event,.user_data=session};
        session->handle=esp_http_client_init(&config);
        if(!session->handle)return NULL;
        char auth[48];snprintf(auth,sizeof(auth),"Bearer %s",key);
        esp_http_client_set_header(session->handle,"Authorization",auth);
    }
    esp_http_client_handle_t h=session->handle;
    if(esp_http_client_set_url(h,url)!=ESP_OK){chant_voice_session_finish(session,false);return NULL;}
    session->server_close=false;
    session->requests++;
    esp_http_client_set_method(h,upload?HTTP_METHOD_POST:HTTP_METHOD_GET);
    esp_http_client_set_timeout_ms(h,15000);
    /* The same client changes from POST to GET. Never carry entity or upload
     * admission headers into polling, cancellation, bitmap or audio requests. */
    const char *headers[]={"Content-Type","Content-Length","Transfer-Encoding",
        "X-Passport-Request-ID","X-Passport-Replace-Ready","Accept"};
    for(unsigned i=0;i<sizeof(headers)/sizeof(headers[0]);i++)esp_http_client_delete_header(h,headers[i]);
    if(upload)esp_http_client_set_header(h,"Content-Type","application/octet-stream");
    if(upload && request_id){
        esp_http_client_set_header(h,"X-Passport-Request-ID",request_id);
        esp_http_client_set_header(h,"X-Passport-Replace-Ready","1");
    }
    if(accept)esp_http_client_set_header(h,"Accept",accept);
    return h;
}

void chant_voice_session_finish(chant_voice_session_t *session, bool success)
{
    if(session->handle && (!success || session->server_close ||
        !esp_http_client_is_complete_data_received(session->handle)))
        esp_http_client_close(session->handle);
}

void chant_voice_session_destroy(chant_voice_session_t *session)
{
    if(session->handle){
        esp_http_client_close(session->handle);
        esp_http_client_cleanup(session->handle);
        session->handle=NULL;
    }
}
