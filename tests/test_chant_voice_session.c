/* Run the production HTTP session adapter against a transport double. Verify
 * handle lifetime and request isolation across a complete voice transaction. */
#include "chant_voice_session.h"
#include "esp_crt_bundle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake_http {
    esp_http_client_config_t config;
    bool complete, connected;
    int method, timeout;
    char names[12][48], values[12][80];
};
static struct fake_http transport;
static unsigned initialized, closed, cleaned;
static bool fail_init, fail_url;
void esp_crt_bundle_attach(void){}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config){
    if(fail_init)return NULL;
    assert(config->crt_bundle_attach==esp_crt_bundle_attach);
    assert(config->disable_auto_redirect);
    memset(&transport,0,sizeof(transport));transport.config=*config;initialized++;return &transport;
}
esp_err_t esp_http_client_set_url(esp_http_client_handle_t h,const char *url){assert(h&&url);return fail_url?ESP_FAIL:ESP_OK;}
int esp_http_client_set_method(esp_http_client_handle_t h,int method){h->method=method;return ESP_OK;}
int esp_http_client_set_timeout_ms(esp_http_client_handle_t h,int ms){h->timeout=ms;return ESP_OK;}
int esp_http_client_set_header(esp_http_client_handle_t h,const char *name,const char *value){
    for(int i=0;i<12;i++)if(!h->names[i][0]||!strcmp(name,h->names[i])){
        snprintf(h->names[i],48,"%s",name);snprintf(h->values[i],80,"%s",value);return ESP_OK;
    }
    assert(0);return ESP_FAIL;
}
int esp_http_client_delete_header(esp_http_client_handle_t h,const char *name){
    for(int i=0;i<12;i++)if(!strcmp(name,h->names[i]))h->names[i][0]=h->values[i][0]=0;
    return ESP_OK;
}
static const char *header(esp_http_client_handle_t h,const char *name){
    for(int i=0;i<12;i++)if(!strcmp(name,h->names[i]))return h->values[i];
    return NULL;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h){return h->complete;}
int esp_http_client_close(esp_http_client_handle_t h){h->connected=false;closed++;return ESP_OK;}
int esp_http_client_cleanup(esp_http_client_handle_t h){assert(!h->connected);cleaned++;return ESP_OK;}
static void received(chant_voice_session_t *s){s->handle->connected=true;s->handle->complete=true;chant_voice_session_finish(s,true);}
int main(void){
    chant_voice_session_t s={0};
    fail_init=true;assert(!chant_voice_session_prepare(&s,"https://example.com/v1/voice",true,NULL,"0123","test-key"));
    chant_voice_session_destroy(&s);assert(!cleaned);fail_init=false;
    esp_http_client_handle_t h=chant_voice_session_prepare(&s,"https://example.com/v1/voice",true,NULL,"0123","test-key");
    assert(h&&h->method==HTTP_METHOD_POST&&h->timeout==15000);
    assert(!strcmp(header(h,"Authorization"),"Bearer test-key"));
    assert(!strcmp(header(h,"X-Passport-Request-ID"),"0123"));
    received(&s);assert(h->connected&&!closed);
    esp_http_client_set_header(h,"Content-Length","1234");
    esp_http_client_set_header(h,"Transfer-Encoding","chunked");
    for(int poll=0;poll<20;poll++){
        assert(chant_voice_session_prepare(&s,"https://example.com/v1/jobs/0123",false,NULL,NULL,"test-key")==h);
        assert(h->method==HTTP_METHOD_GET&&!header(h,"Content-Length")&&!header(h,"Transfer-Encoding"));
        assert(!header(h,"Content-Type")&&!header(h,"X-Passport-Request-ID")&&!header(h,"X-Passport-Replace-Ready"));
        assert(!strcmp(header(h,"Authorization"),"Bearer test-key"));
        received(&s);
    }
    assert(initialized==1&&closed==0&&s.requests==21);
    chant_voice_session_prepare(&s,"https://example.com/audio",false,"audio/x-passport-ima-adpcm",NULL,"test-key");
    assert(header(h,"Accept"));received(&s);
    chant_voice_session_prepare(&s,"https://example.com/text",false,NULL,NULL,"test-key");
    assert(!header(h,"Accept"));
    /* Truncated or failed responses must close before retry; a complete one
     * must also honor an explicit server Connection: close. */
    h->complete=false;chant_voice_session_finish(&s,true);assert(closed==1);
    chant_voice_session_prepare(&s,"https://example.com/text",false,NULL,NULL,"test-key");
    received(&s);chant_voice_session_finish(&s,false);assert(closed==2);
    chant_voice_session_prepare(&s,"https://example.com/text",false,NULL,NULL,"test-key");
    esp_http_client_event_t event={HTTP_EVENT_ON_HEADER,&s,"connection","Close"};
    h->config.event_handler(&event);received(&s);assert(closed==3&&!h->connected);
    chant_voice_session_prepare(&s,"https://example.com/text",false,NULL,NULL,"test-key");
    received(&s);assert(h->connected&&closed==3);
    fail_url=true;assert(!chant_voice_session_prepare(&s,"bad",false,NULL,NULL,"test-key"));assert(closed==4);fail_url=false;
    chant_voice_session_destroy(&s);assert(!s.handle&&cleaned==1);
    chant_voice_session_destroy(&s);assert(cleaned==1);
    /* A subsequent recording gets a new session instead of keeping TLS RAM
     * allocated while microphone capture runs. */
    chant_voice_session_prepare(&s,"https://example.com/v1/voice",true,NULL,"4567","test-key");
    assert(initialized==2&&!strcmp(header(s.handle,"X-Passport-Request-ID"),"4567"));
    chant_voice_session_destroy(&s);assert(cleaned==2);
    puts("Voice HTTP session reuse, header isolation and failure cleanup: PASS");
}
