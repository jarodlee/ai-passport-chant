#include "chant_wifi_profiles.h"
#include "chant_services_model.h"
#include <string.h>
void chant_wifi_profiles_init(chant_wifi_profiles_t *p){memset(p,0,sizeof(*p));p->version=CHANT_WIFI_PROFILE_VERSION;}
bool chant_wifi_profiles_valid(const chant_wifi_profiles_t *p){
 if(!p||p->version!=CHANT_WIFI_PROFILE_VERSION||p->count>CHANT_WIFI_MAX_PROFILES||p->active>=(p->count?p->count:1))return false;
 for(unsigned i=0;i<p->count;i++){
  if(!memchr(p->entries[i].ssid,0,sizeof(p->entries[i].ssid))||!memchr(p->entries[i].password,0,sizeof(p->entries[i].password))||!chant_wifi_credentials_valid(p->entries[i].ssid,p->entries[i].password))return false;
  for(unsigned j=0;j<i;j++)if(!strcmp(p->entries[i].ssid,p->entries[j].ssid))return false;
 }
 return true;
}
int chant_wifi_profiles_put(chant_wifi_profiles_t *p,const char *ssid,const char *password){
 if(!chant_wifi_profiles_valid(p)||!chant_wifi_credentials_valid(ssid,password))return -1;
 unsigned i=0;while(i<p->count&&strcmp(p->entries[i].ssid,ssid))i++;
 if(i==p->count){if(p->count==CHANT_WIFI_MAX_PROFILES)return -1;p->count++;}
 memset(&p->entries[i],0,sizeof(p->entries[i]));memcpy(p->entries[i].ssid,ssid,strlen(ssid));memcpy(p->entries[i].password,password,strlen(password));return (int)i;
}
void chant_wifi_profiles_next(chant_wifi_profiles_t *p){if(chant_wifi_profiles_valid(p)&&p->count)p->active=(p->active+1)%p->count;}
