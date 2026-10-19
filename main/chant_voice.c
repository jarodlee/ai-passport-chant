#include "chant_voice.h"
#include "chant_voice_control.h"
#include "chant_voice_http.h"
#include "chant_voice_session.h"
#include "chant_voice_endpoint.h"
#include "chant_voice_stream.h"
#include "chant_adpcm.h"
#if __has_include("chant_bridge_local.h")
#include "chant_bridge_local.h"
#else
/* The pairing key is local-only (gitignored); a placeholder keeps the repo buildable. */
#define CHANT_BRIDGE_KEY "00000000000000000000000000000000"
#endif
#ifndef CHANT_BRIDGE_REMOTE_URL
#define CHANT_BRIDGE_REMOTE_URL ""
#endif
#include "chant_network.h"
#include "bsp_audio.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <time.h>
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "mbedtls/md.h"
#include "cJSON.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
/* Only the audio task touches codec/PCM and runs this bounded transaction. */
static chant_voice_control_t control;
/* Audio-owner-only deferred cleanup: record the next utterance before network I/O. */
static char pending_cancel_url[256];
static atomic_int phase;
static atomic_int failure_phase=8;
static uint8_t reply_pixels[CHANT_REPLY_PALETTE_BYTES+CHANT_REPLY_BYTES] __attribute__((aligned(4)));
static uint8_t reply_staging[CHANT_REPLY_BYTES];
static atomic_bool reply_ready;
static atomic_uint reply_serial;
static const char *labels[]={"按住确定说话\n松开发送 最长八秒", "正在录音\n松开发送 最长八秒", "正在查找本机Cindy", "正在发送语音", "Cindy正在思考", "Cindy正在回复", "请先连接Wi-Fi", "未找到本机桥接\n请打开Mac上的Cindy", "语音请求失败\n请重试或检查网络", "说话太短 请重试", "内存不足 请重试", "配对校验失败\n请检查桥接配置", "远程入口拒绝访问\n请检查网络", "上一段对话未结束\n请稍后再问", "请求过于频繁\n请稍后再问", "远程服务暂不可用\n请稍后再问", "网络响应超时\n请重试", "Cindy回答失败\n请查看语音对话任务", "Cindy回答超时\n请稍后再问"};
const char *chant_voice_status(void){return labels[atomic_load(&phase)];}
void chant_voice_press(bool down){chant_voice_control_press(&control,down);if(down)chant_voice_clear_reply();}
bool chant_voice_requested(void){return chant_voice_control_requested(&control);}
bool chant_voice_busy(void){return chant_voice_control_busy(&control);}
bool chant_voice_reply(const uint8_t **pixels,unsigned *serial){if(!atomic_load(&reply_ready))return false;*pixels=reply_pixels;*serial=atomic_load(&reply_serial);return true;}
void chant_voice_clear_reply(void){atomic_store(&reply_ready,false);atomic_fetch_add(&reply_serial,1);}
void chant_voice_cancel(void){chant_voice_control_cancel(&control);}
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
static int json_read(void *h,char *out,size_t size){
 int n=esp_http_client_read(h,out,(int)size);return n==-ESP_ERR_HTTP_EAGAIN?CHANT_HTTP_AGAIN:n;
}
static bool json_complete(void *h){return esp_http_client_is_complete_data_received(h);}
static uint64_t json_time(void *h){(void)h;return esp_timer_get_time()/1000;}
static bool json_cancelled(void *h){(void)h;return chant_voice_control_cancelled(&control);}
static void json_pause(void *h){(void)h;vTaskDelay(pdMS_TO_TICKS(20));}
static void http_failure(int status,chant_http_result_t result){
 int p=status==401?11:status==403?12:status==409?13:status==429?14:status>=500?15:result==CHANT_HTTP_TIMEOUT?16:8;
 atomic_store(&failure_phase,p);
}
static bool request_json_id(chant_voice_session_t *session,const char *url,const uint8_t *data,size_t size,char out[384],const char *request_id){
 for(unsigned attempt=0;attempt<2&&!chant_voice_control_cancelled(&control);attempt++){
  out[0]=0;
  esp_http_client_handle_t h=chant_voice_session_prepare(session,url,data!=NULL,NULL,request_id,CHANT_BRIDGE_KEY);
  if(!h){http_failure(0,CHANT_HTTP_READ_ERROR);return false;}
  const char *stage="connect";int status=0;esp_err_t error=ESP_OK;
  chant_http_result_t result=CHANT_HTTP_READ_ERROR;bool ok=false,transport_failure=true;
  esp_http_client_set_timeout_ms(h,15000);
  error=esp_http_client_open(h,(int)size);if(error!=ESP_OK)goto finished;
  stage="write";
  if(data){size_t pos=0;while(pos<size&&!chant_voice_control_cancelled(&control)){
   int n=esp_http_client_write(h,(const char*)data+pos,size-pos>1024?1024:(int)(size-pos));
   if(n<=0){error=n;goto finished;}pos+=(size_t)n;
  }}
  if(chant_voice_control_cancelled(&control)){result=CHANT_HTTP_CANCELLED;goto finished;}
  stage="headers";
  int64_t headers=esp_http_client_fetch_headers(h);if(headers<0){error=(esp_err_t)headers;goto finished;}
  status=esp_http_client_get_status_code(h);stage="status";
  if(status!=200&&status!=202){transport_failure=false;goto finished;}
  stage="body";
  esp_http_client_set_timeout_ms(h,750);
  chant_http_reader_t io={h,json_read,json_complete,json_time,json_cancelled,json_pause};
  result=chant_http_read_body(&io,out,384,10000);
  ok=result==CHANT_HTTP_OK;transport_failure=result==CHANT_HTTP_READ_ERROR||result==CHANT_HTTP_TIMEOUT;
  finished:
  if(!ok)ESP_LOGW("chant_voice","JSON %s attempt=%u stage=%s HTTP=%d error=%ld body=%d heap=%lu",
   data?"upload":"request",attempt+1,stage,status,(long)error,result,(unsigned long)esp_get_free_heap_size());
  chant_voice_session_finish(session,ok);
  if(ok)return true;
  http_failure(status,result);
  if(result==CHANT_HTTP_CANCELLED||!chant_http_retry(attempt,status,transport_failure,data!=NULL,request_id!=NULL))break;
  for(int i=0;i<10&&!chant_voice_control_cancelled(&control);i++)vTaskDelay(pdMS_TO_TICKS(50));
 }
 return false;
}
static bool request_json(chant_voice_session_t *session,const char *url,const uint8_t *data,size_t size,char out[384]){
 return request_json_id(session,url,data,size,out,NULL);
}
/* The network producer never accesses the codec. The audio owner consumes a
 * bounded compressed queue, so TLS/network waits cannot starve each PCM write. */
typedef struct {
 const char *url;
 chant_voice_session_t *session;
 StreamBufferHandle_t queue;
 SemaphoreHandle_t done;
 atomic_bool stop,finished,failed;
 atomic_uint samples;
} voice_stream_t;

static void download_speech(void *arg){
 voice_stream_t *s=arg;
 esp_http_client_handle_t h=NULL;
 bool ok=false;
 uint8_t chunk[1024],header[CHANT_VOICE_HEADER_BYTES];
 int64_t end=esp_timer_get_time()+120000000;
 int64_t bytes=0;
 /* Retry before consuming audio only. Once playback starts, never replay it. */
 for(unsigned attempt=0;attempt<2;attempt++){
  if(atomic_load(&s->stop)||chant_voice_control_cancelled(&control))goto done;
  h=chant_voice_session_prepare(s->session,s->url,false,"audio/x-passport-ima-adpcm",NULL,CHANT_BRIDGE_KEY);
  if(!h)goto done;
  esp_err_t error=esp_http_client_open(h,0);
  bytes=error==ESP_OK?esp_http_client_fetch_headers(h):-1;
  int status=bytes>=0?esp_http_client_get_status_code(h):0;
  if(status==200&&bytes>CHANT_VOICE_HEADER_BYTES)break;
  chant_voice_session_finish(s->session,false);
  if(!chant_http_retry(attempt,status,bytes<0,false,false))goto done;
  vTaskDelay(pdMS_TO_TICKS(500));
 }
 /* Bound a stalled body read during playback, without shortening TLS setup. */
 esp_http_client_set_timeout_ms(h,250);
 size_t used=0;
 while(used<sizeof(header)&&!atomic_load(&s->stop)&&!chant_voice_control_cancelled(&control)&&esp_timer_get_time()<end){
  int n=esp_http_client_read(h,(char*)header+used,sizeof(header)-used);
  if(n==-ESP_ERR_HTTP_EAGAIN){vTaskDelay(1);continue;}
  if(n<0)goto done;
  if(!n){if(esp_http_client_is_complete_data_received(h))goto done;vTaskDelay(1);continue;}
  used+=n;
 }
 uint32_t samples;
 if(used!=sizeof(header)||!chant_voice_stream_header(header,bytes,&samples))goto done;
 atomic_store(&s->samples,samples);
 int64_t total=sizeof(header);
 while(total<bytes&&!atomic_load(&s->stop)&&!chant_voice_control_cancelled(&control)&&esp_timer_get_time()<end){
  int want=bytes-total>(int64_t)sizeof(chunk)?sizeof(chunk):(int)(bytes-total);
  int n=esp_http_client_read(h,(char*)chunk,want);
  if(n==-ESP_ERR_HTTP_EAGAIN){vTaskDelay(1);continue;}
  if(n<0)goto done;
  if(!n){if(esp_http_client_is_complete_data_received(h))break;vTaskDelay(1);continue;}
  total+=n;
  size_t sent=0;
  while(sent<(size_t)n&&!atomic_load(&s->stop)&&!chant_voice_control_cancelled(&control)&&esp_timer_get_time()<end)
   sent+=xStreamBufferSend(s->queue,chunk+sent,n-sent,pdMS_TO_TICKS(50));
  if(sent!=(size_t)n)goto done;
 }
 ok=total==bytes&&esp_http_client_is_complete_data_received(h)&&!atomic_load(&s->stop)&&!chant_voice_control_cancelled(&control);
 done:
 chant_voice_session_finish(s->session,ok);
 atomic_store(&s->failed,!ok);
 atomic_store(&s->finished,true);
 /* No more accesses to s after signalling: its storage belongs to the caller. */
 xSemaphoreGive(s->done);
 vTaskDelete(NULL);
}

static bool speak(chant_voice_session_t *session,const char *url){
 voice_stream_t stream={.url=url,.session=session};
 stream.queue=xStreamBufferCreate(CHANT_VOICE_STREAM_BYTES,1);
 stream.done=xSemaphoreCreateBinary();
 bool ok=false;
 if(!stream.queue||!stream.done)goto done;
 if(xTaskCreate(download_speech,"voice_download",6144,&stream,4,NULL)!=pdPASS)goto done;
 uint8_t encoded[256];int16_t pcm[512];chant_decoder_t decoder={0};
 uint32_t played=0;unsigned rebuffers=0;bool buffering=true,started=false;
 int64_t end=esp_timer_get_time()+125000000;
 while(!chant_voice_control_cancelled(&control)&&esp_timer_get_time()<end){
  if(atomic_load(&stream.failed))break;
  bool finished=atomic_load(&stream.finished);
  size_t available=xStreamBufferBytesAvailable(stream.queue);
  uint32_t samples=atomic_load(&stream.samples);
  if(finished&&played==samples&&samples){ok=true;break;}
  /* Two seconds before starting; after an underrun refill instead of writing
   * tiny fragments. A fully downloaded short reply starts immediately. */
  if(buffering){
   if(!finished&&available<CHANT_VOICE_PREBUFFER_BYTES){vTaskDelay(pdMS_TO_TICKS(20));continue;}
   if(!available)break;
   buffering=false;started=true;atomic_store(&phase,5);
  }
  if(!finished&&available<sizeof(encoded)){
   buffering=true;rebuffers++;continue;
  }
  size_t n=xStreamBufferReceive(stream.queue,encoded,sizeof(encoded),0);
  if(!n){if(finished)break;continue;}
  size_t count=n*2;
  if(count>samples-played)count=samples-played;
  chant_decode(&decoder,encoded,0,count,pcm);
  if(bsp_audio_write(pcm,count*sizeof(*pcm))!=ESP_OK)break;
  played+=count;
 }
 atomic_store(&stream.stop,true);
 /* Joining before releasing the queue/context prevents cancellation races. */
 xSemaphoreTake(stream.done,portMAX_DELAY);
 ok=ok&&!atomic_load(&stream.failed)&&!chant_voice_control_cancelled(&control);
 ESP_LOGI("chant_voice","Buffered speech: %lu samples, started=%d refills=%u %s",(unsigned long)played,started,rebuffers,ok?"PASS":"FAIL");
 done:
 if(stream.queue)vStreamBufferDelete(stream.queue);
 if(stream.done)vSemaphoreDelete(stream.done);
 return ok;
}
static bool fetch_reply(chant_voice_session_t *session,const char *url){
 esp_http_client_handle_t h=chant_voice_session_prepare(session,url,false,NULL,NULL,CHANT_BRIDGE_KEY);if(!h)return false;bool ok=false;
 if(esp_http_client_open(h,0)!=ESP_OK)goto done;
 int64_t bytes=esp_http_client_fetch_headers(h);
 if(esp_http_client_get_status_code(h)!=200||bytes!=CHANT_REPLY_BYTES)goto done;
 esp_http_client_set_timeout_ms(h,750);
 int used=0;int64_t end=esp_timer_get_time()+8000000;
 while(used<CHANT_REPLY_BYTES&&!chant_voice_control_cancelled(&control)&&esp_timer_get_time()<end){int n=esp_http_client_read(h,(char*)reply_staging+used,CHANT_REPLY_BYTES-used);if(n==-ESP_ERR_HTTP_EAGAIN)continue;if(n<0)goto done;if(n==0){if(esp_http_client_is_complete_data_received(h))break;continue;}used+=n;}
 ok=used==CHANT_REPLY_BYTES&&esp_http_client_is_complete_data_received(h);
 done:chant_voice_session_finish(session,ok);
 if(ok){memcpy(reply_pixels+CHANT_REPLY_PALETTE_BYTES,reply_staging,CHANT_REPLY_BYTES);atomic_fetch_add(&reply_serial,1);atomic_store(&reply_ready,true);}
 return ok;
}
void chant_voice_run(uint8_t volume){
 if(!chant_voice_control_begin(&control))return;
 chant_voice_session_t session={0};
 atomic_store(&failure_phase,8);
 chant_voice_clear_reply();chant_network_state_t net;chant_network_snapshot(&net);
 if(net.status!=NET_CONNECTED){atomic_store(&phase,6);goto done;}
 if(!chant_voice_control_held(&control)){/* A quick release records nothing; never open an unintended mic. */atomic_store(&phase,9);goto done;}
 uint8_t *data=malloc(64000);if(!data){atomic_store(&phase,10);goto done;}
 int16_t pcm[512]={0};int predictor=0,index=0;size_t samples=0;
 /* Record first: discovery can take seconds, and speech must not be lost meanwhile. */
 bool ok=bsp_audio_set_format(16000,16,1)==ESP_OK;atomic_store(&phase,1);
 /* Drain stale microphone DMA while output is silent. */
 for(int i=0;ok&&i<4;i++){memset(pcm,0,sizeof(pcm));ok=bsp_audio_write(pcm,sizeof(pcm))==ESP_OK&&bsp_audio_read(pcm,sizeof(pcm))==ESP_OK;}
 int64_t until=esp_timer_get_time()+8000000;
 while(ok&&!chant_voice_control_cancelled(&control)&&chant_voice_control_held(&control)&&samples<128000&&esp_timer_get_time()<until){memset(pcm,0,sizeof(pcm));ok=bsp_audio_write(pcm,sizeof(pcm))==ESP_OK&&bsp_audio_read(pcm,sizeof(pcm))==ESP_OK;if(!ok)break;for(size_t i=0;i<512&&samples<128000;i++,samples++){unsigned n=chant_encode_sample(pcm[i],&predictor,&index);if(samples&1)data[samples/2]|=n;else data[samples/2]=n<<4;}}
 ESP_LOGI("chant_voice","Recorded %u samples",(unsigned)samples);
 if(!ok||samples<3200){free(data);atomic_store(&phase,chant_voice_control_cancelled(&control)?0:(ok?9:8));goto done;}
 if(chant_voice_control_cancelled(&control)){free(data);atomic_store(&phase,0);goto done;}
 size_t encoded_size=(samples+1)/2;
 uint8_t *compact=realloc(data,encoded_size);if(compact)data=compact;
 /* Cleaning up the old job must never delay opening the microphone. */
 if(pending_cancel_url[0]){char ignored[384];(void)request_json(&session,pending_cancel_url,NULL,0,ignored);pending_cancel_url[0]=0;}
 atomic_store(&failure_phase,8);
 atomic_store(&phase,2);char ip[16],base[CHANT_VOICE_BASE_SIZE];
 bool remote=false;
 if(discover(ip))snprintf(base,sizeof(base),"http://%s:18765",ip);
 else if(chant_voice_remote_base(CHANT_BRIDGE_REMOTE_URL,base))remote=true;
 else{free(data);atomic_store(&phase,7);goto done;}
 /* Certificate checks need the clock supplied by the existing SNTP service. */
 if(remote&&time(NULL)<1700000000){free(data);atomic_store(&phase,8);goto done;}
 if(chant_voice_control_cancelled(&control)){free(data);atomic_store(&phase,0);goto done;}
 ESP_LOGI("chant_voice","Bridge route: %s",remote?"HTTPS":"LAN");
 atomic_store(&phase,3);char url[256],response[384],jobid[17]={0},request_id[17];
 snprintf(request_id,sizeof(request_id),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
 snprintf(url,sizeof(url),"%s/v1/voice",base);ok=request_json_id(&session,url,data,encoded_size,response,request_id);free(data);
 if(ok){cJSON *j=cJSON_Parse(response);cJSON *id=cJSON_GetObjectItem(j,"jobId");if(cJSON_IsString(id)&&strlen(id->valuestring)==16&&strspn(id->valuestring,"0123456789abcdef")==16)memcpy(jobid,id->valuestring,16);cJSON_Delete(j);ok=jobid[0]!=0;if(!ok)ESP_LOGW("chant_voice","Upload JSON has no valid job identifier");}
 ESP_LOGI("chant_voice","Upload %s",ok?"accepted":"failed");
 atomic_store(&phase,4);until=esp_timer_get_time()+130000000;bool ready=false;char state[16]="";
 while(ok&&!chant_voice_control_cancelled(&control)&&esp_timer_get_time()<until){vTaskDelay(pdMS_TO_TICKS(700));snprintf(url,sizeof(url),"%s/v1/jobs/%s",base,jobid);ok=request_json(&session,url,NULL,0,response);if(!ok)break;cJSON *j=cJSON_Parse(response),*s=cJSON_GetObjectItem(j,"state");if(!cJSON_IsString(s)){ok=false;}else{snprintf(state,sizeof(state),"%s",s->valuestring);if(!strcmp(state,"ready")){ready=true;}else if(!strcmp(state,"error")){cJSON *e=cJSON_GetObjectItem(j,"error");atomic_store(&failure_phase,cJSON_IsString(e)&&!strcmp(e->valuestring,"TIMEOUT")?18:17);ok=false;}}cJSON_Delete(j);if(ready)break;}
 if(ok&&ready&&!chant_voice_control_cancelled(&control)){
  atomic_store(&phase,5);snprintf(url,sizeof(url),"%s/v1/jobs/%s/text",base,jobid);
  bool have_text=fetch_reply(&session,url);ESP_LOGI("chant_voice","Reply text bitmap: %s",have_text?"ready":"unavailable");
  snprintf(url,sizeof(url),"%s/v1/jobs/%s/audio",base,jobid);bsp_audio_set_volume(volume);ok=speak(&session,url);
 }
 else{ok=false;if(jobid[0])ESP_LOGI("chant_voice","Job ended in state %s",state);}
 if((chant_voice_control_cancelled(&control)||!ok)&&jobid[0]){
  snprintf(pending_cancel_url,sizeof(pending_cancel_url),"%s/v1/jobs/%s/cancel",base,jobid);
 }
 bool was_cancelled=chant_voice_control_cancelled(&control);
 atomic_store(&phase,was_cancelled?0:(ok?0:atomic_load(&failure_phase)));
 ESP_LOGI("chant_voice","Conversation %s",was_cancelled?"cancelled":(ok?"PASS":"FAIL"));
 done:
 if(session.requests)ESP_LOGI("chant_voice","Voice HTTP: requests=%u connections=%u",session.requests,session.connections);
 chant_voice_session_destroy(&session);chant_voice_control_finish(&control);
}
