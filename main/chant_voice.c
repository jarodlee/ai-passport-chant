#include "chant_voice.h"
#include "chant_adpcm.h"
#include "chant_bridge_local.h"
#include "chant_network.h"
#include "bsp_audio.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "mbedtls/md.h"
#include "cJSON.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
/* Only the audio task touches codec/PCM and runs this bounded transaction. */
static atomic_bool held,requested,busy;
static atomic_int phase;
static atomic_bool cancelled;
static uint8_t reply_pixels[CHANT_REPLY_PALETTE_BYTES+CHANT_REPLY_BYTES] __attribute__((aligned(4)));
static uint8_t reply_staging[CHANT_REPLY_BYTES];
static atomic_bool reply_ready;
static atomic_uint reply_serial;
static const char *labels[]={"按住确定说话\n松开发送 最长八秒", "正在录音\n松开发送 最长八秒", "正在查找本机Cindy", "正在发送语音", "Cindy正在思考", "Cindy正在回复", "请先连接Wi-Fi", "未找到本机桥接\n请打开Mac上的Cindy", "对话失败 请重试\n检查本机桥接状态", "说话太短 请重试", "内存不足 请重试"};
const char *chant_voice_status(void){return labels[atomic_load(&phase)];}
void chant_voice_press(bool down){atomic_store(&held,down);if(down&&!atomic_load(&busy))atomic_store(&requested,true);}
bool chant_voice_requested(void){return atomic_exchange(&requested,false);}
bool chant_voice_busy(void){return atomic_load(&busy)||atomic_load(&requested);}
bool chant_voice_reply(const uint8_t **pixels,unsigned *serial){if(!atomic_load(&reply_ready))return false;*pixels=reply_pixels;*serial=atomic_load(&reply_serial);return true;}
void chant_voice_clear_reply(void){atomic_store(&reply_ready,false);atomic_fetch_add(&reply_serial,1);}
void chant_voice_cancel(void){atomic_store(&cancelled,true);}
static void hmac(const char *s,char out[65]){unsigned char sum[32];mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),(const unsigned char*)CHANT_BRIDGE_KEY,strlen(CHANT_BRIDGE_KEY),(const unsigned char*)s,strlen(s),sum);for(int i=0;i<32;i++)snprintf(out+2*i,3,"%02x",sum[i]);}
static bool discover(char ip[16]){
 int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_IP);if(fd<0)return false;
 int yes=1;setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));struct timeval timeout={.tv_sec=1};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
 char nonce[17],signature[65],query[100],reply[120],expected[100],value[32];snprintf(nonce,sizeof(nonce),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());hmac(nonce,signature);snprintf(query,sizeof(query),"PASSPORT %s %s",nonce,signature);snprintf(value,sizeof(value),"%s reply",nonce);hmac(value,signature);snprintf(expected,sizeof(expected),"PASSPORT %s %s",nonce,signature);
 struct sockaddr_in dest={.sin_family=AF_INET,.sin_port=htons(18766),.sin_addr.s_addr=INADDR_BROADCAST};bool found=false;
 for(int attempt=0;attempt<3&&!found;attempt++){
  if(sendto(fd,query,strlen(query),0,(struct sockaddr*)&dest,sizeof(dest))<0)break;
  struct sockaddr_in source;socklen_t length=sizeof(source);int n=recvfrom(fd,reply,sizeof(reply)-1,0,(struct sockaddr*)&source,&length);
  if(n>0){reply[n]=0;if(!strcmp(reply,expected)&&source.sin_port==htons(18766)){inet_ntoa_r(source.sin_addr,ip,16);found=true;}}
 }
 close(fd);return found;
}
static esp_http_client_handle_t client(const char *url,const char *method){esp_http_client_config_t cfg={.url=url,.timeout_ms=6000,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true,.keep_alive_enable=false};esp_http_client_handle_t h=esp_http_client_init(&cfg);if(!h)return NULL;char auth[48];snprintf(auth,sizeof(auth),"Bearer %s",CHANT_BRIDGE_KEY);esp_http_client_set_header(h,"Authorization",auth);esp_http_client_set_method(h,!strcmp(method,"POST")?HTTP_METHOD_POST:HTTP_METHOD_GET);return h;}
static bool request_json(const char *url,const uint8_t *data,size_t size,char out[384]){
 esp_http_client_handle_t h=client(url,data?"POST":"GET");if(!h)return false;bool ok=false;
 if(data)esp_http_client_set_header(h,"Content-Type","application/octet-stream");
 if(esp_http_client_open(h,size)!=ESP_OK)goto done;
 if(data){size_t pos=0;while(pos<size){int n=esp_http_client_write(h,(const char*)data+pos,size-pos>1024?1024:(int)(size-pos));if(n<=0)goto done;pos+=n;}}
 if(esp_http_client_fetch_headers(h)<0)goto done;
 int status=esp_http_client_get_status_code(h);if(status!=200&&status!=202)goto done;
 int used=0;int64_t deadline=esp_timer_get_time()+8000000;
 while(used<383&&esp_timer_get_time()<deadline){int n=esp_http_client_read(h,out+used,383-used);if(n<0)goto done;if(!n){if(esp_http_client_is_complete_data_received(h))break;continue;}used+=n;}
 out[used]=0;ok=esp_http_client_is_complete_data_received(h);
 done:esp_http_client_close(h);esp_http_client_cleanup(h);return ok;
}
static bool speak(const char *url){
 esp_http_client_handle_t h=client(url,"GET");if(!h)return false;bool ok=false;int16_t pcm[512];
 if(esp_http_client_open(h,0)!=ESP_OK)goto done;
 int64_t bytes=esp_http_client_fetch_headers(h);if(esp_http_client_get_status_code(h)!=200||bytes<=0||bytes>2880000||(bytes&1))goto done;
 int64_t end=esp_timer_get_time()+100000000,total=0;size_t used=0;atomic_store(&phase,5);
 while(total<bytes&&esp_timer_get_time()<end&&!atomic_load(&cancelled)){int n=esp_http_client_read(h,(char*)pcm+used,sizeof(pcm)-used);if(n<0)goto done;if(n==0){if(esp_http_client_is_complete_data_received(h))break;continue;}used+=n;total+=n;size_t count=used&~(size_t)1;if(count&&bsp_audio_write(pcm,count)!=ESP_OK)goto done;if(used&1)((char*)pcm)[0]=((char*)pcm)[used-1];used&=1;}
 ok=total==bytes&&used==0&&!atomic_load(&cancelled);
 done:esp_http_client_close(h);esp_http_client_cleanup(h);return ok;
}
static bool fetch_reply(const char *url){
 esp_http_client_handle_t h=client(url,"GET");if(!h)return false;bool ok=false;
 if(esp_http_client_open(h,0)!=ESP_OK)goto done;
 int64_t bytes=esp_http_client_fetch_headers(h);
 if(esp_http_client_get_status_code(h)!=200||bytes!=CHANT_REPLY_BYTES)goto done;
 int used=0;int64_t end=esp_timer_get_time()+8000000;
 while(used<CHANT_REPLY_BYTES&&esp_timer_get_time()<end){int n=esp_http_client_read(h,(char*)reply_staging+used,CHANT_REPLY_BYTES-used);if(n<0)goto done;if(n==0){if(esp_http_client_is_complete_data_received(h))break;continue;}used+=n;}
 ok=used==CHANT_REPLY_BYTES&&esp_http_client_is_complete_data_received(h);
 done:esp_http_client_close(h);esp_http_client_cleanup(h);
 if(ok){memcpy(reply_pixels+CHANT_REPLY_PALETTE_BYTES,reply_staging,CHANT_REPLY_BYTES);atomic_fetch_add(&reply_serial,1);atomic_store(&reply_ready,true);}
 return ok;
}
void chant_voice_run(uint8_t volume){
 atomic_store(&busy,true);atomic_store(&cancelled,false);chant_voice_clear_reply();chant_network_state_t net;chant_network_snapshot(&net);
 if(net.status!=NET_CONNECTED){atomic_store(&phase,6);goto done;}
 if(!atomic_load(&held)){/* A quick release records nothing; never open an unintended mic. */atomic_store(&phase,9);goto done;}
 uint8_t *data=malloc(64000);if(!data){atomic_store(&phase,10);goto done;}
 int16_t pcm[512]={0};int predictor=0,index=0;size_t samples=0;
 /* Record first: discovery can take seconds, and speech must not be lost meanwhile. */
 bool ok=bsp_audio_set_format(16000,16,1)==ESP_OK;atomic_store(&phase,1);
 /* Drain stale microphone DMA while output is silent. */
 for(int i=0;ok&&i<4;i++){memset(pcm,0,sizeof(pcm));ok=bsp_audio_write(pcm,sizeof(pcm))==ESP_OK&&bsp_audio_read(pcm,sizeof(pcm))==ESP_OK;}
 int64_t until=esp_timer_get_time()+8000000;
 while(ok&&!atomic_load(&cancelled)&&atomic_load(&held)&&samples<128000&&esp_timer_get_time()<until){memset(pcm,0,sizeof(pcm));ok=bsp_audio_write(pcm,sizeof(pcm))==ESP_OK&&bsp_audio_read(pcm,sizeof(pcm))==ESP_OK;if(!ok)break;for(size_t i=0;i<512&&samples<128000;i++,samples++){unsigned n=chant_encode_sample(pcm[i],&predictor,&index);if(samples&1)data[samples/2]|=n;else data[samples/2]=n<<4;}}
 ESP_LOGI("chant_voice","Recorded %u samples",(unsigned)samples);
 if(!ok||samples<3200){free(data);atomic_store(&phase,atomic_load(&cancelled)?0:(ok?9:8));goto done;}
 if(atomic_load(&cancelled)){free(data);atomic_store(&phase,0);goto done;}
 atomic_store(&phase,2);char ip[16];if(!discover(ip)){free(data);atomic_store(&phase,7);goto done;}
 if(atomic_load(&cancelled)){free(data);atomic_store(&phase,0);goto done;}
 ESP_LOGI("chant_voice","Bridge %s",ip);
 atomic_store(&phase,3);char url[128],response[384],jobid[17]={0};snprintf(url,sizeof(url),"http://%s:18765/v1/voice",ip);ok=request_json(url,data,(samples+1)/2,response);free(data);
 if(ok){cJSON *j=cJSON_Parse(response);cJSON *id=cJSON_GetObjectItem(j,"jobId");if(cJSON_IsString(id)&&strlen(id->valuestring)==16&&strspn(id->valuestring,"0123456789abcdef")==16)memcpy(jobid,id->valuestring,16);cJSON_Delete(j);ok=jobid[0]!=0;}
 ESP_LOGI("chant_voice","Upload %s",ok?"accepted":"failed");
 atomic_store(&phase,4);until=esp_timer_get_time()+130000000;bool ready=false;char state[16]="";
 while(ok&&!atomic_load(&cancelled)&&esp_timer_get_time()<until){vTaskDelay(pdMS_TO_TICKS(700));snprintf(url,sizeof(url),"http://%s:18765/v1/jobs/%s",ip,jobid);ok=request_json(url,NULL,0,response);if(!ok)break;cJSON *j=cJSON_Parse(response),*s=cJSON_GetObjectItem(j,"state");if(!cJSON_IsString(s)){ok=false;}else{snprintf(state,sizeof(state),"%s",s->valuestring);if(!strcmp(state,"ready")){ready=true;}else if(!strcmp(state,"error")){ok=false;}}cJSON_Delete(j);if(ready)break;}
 if(ok&&ready&&!atomic_load(&cancelled)){
  atomic_store(&phase,5);snprintf(url,sizeof(url),"http://%s:18765/v1/jobs/%s/text",ip,jobid);
  bool have_text=fetch_reply(url);ESP_LOGI("chant_voice","Reply text bitmap: %s",have_text?"ready":"unavailable");
  snprintf(url,sizeof(url),"http://%s:18765/v1/jobs/%s/audio",ip,jobid);bsp_audio_set_volume(volume);ok=speak(url);
 }
 else if(jobid[0]) ESP_LOGI("chant_voice","Job ended in state %s",state);
 if(atomic_load(&cancelled)&&jobid[0]){
  snprintf(url,sizeof(url),"http://%s:18765/v1/jobs/%s/cancel",ip,jobid);(void)request_json(url,NULL,0,response);
 }
 bool was_cancelled=atomic_load(&cancelled);
 atomic_store(&phase,was_cancelled?0:(ok?0:8));
 ESP_LOGI("chant_voice","Conversation %s",was_cancelled?"cancelled":(ok?"PASS":"FAIL"));
 done:atomic_store(&busy,false);atomic_store(&requested,false);
}
