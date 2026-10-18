#include "chant_wifi_profiles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 chant_wifi_profiles_t p;chant_wifi_profiles_init(&p);assert(chant_wifi_profiles_valid(&p));chant_wifi_profiles_next(&p);assert(p.active==0);
 assert(chant_wifi_profiles_put(&p,"office","office123")==0);assert(chant_wifi_profiles_put(&p,"home","home1234")==1);assert(chant_wifi_profiles_put(&p,"classroom","class123")==2);
 chant_wifi_profiles_next(&p);assert(p.active==1);chant_wifi_profiles_next(&p);assert(p.active==2);chant_wifi_profiles_next(&p);assert(p.active==0);
 assert(chant_wifi_profiles_put(&p,"home","updated123")==1);assert(p.count==3&&!strcmp(p.entries[1].password,"updated123"));
 assert(chant_wifi_profiles_put(&p,"bad","short")==-1&&p.count==3);
 assert(chant_wifi_profiles_put(&p,"4","")==3);assert(chant_wifi_profiles_put(&p,"5","")==4);assert(chant_wifi_profiles_put(&p,"6","")==5);assert(chant_wifi_profiles_put(&p,"7","")==-1&&p.count==6);
 chant_wifi_profiles_t saved=p;saved.count=7;assert(!chant_wifi_profiles_valid(&saved));saved=p;saved.active=6;assert(!chant_wifi_profiles_valid(&saved));saved=p;saved.version=99;assert(!chant_wifi_profiles_valid(&saved));saved=p;memset(saved.entries[1].ssid,'x',sizeof(saved.entries[1].ssid));assert(!chant_wifi_profiles_valid(&saved));saved=p;saved.entries[2]=saved.entries[0];assert(!chant_wifi_profiles_valid(&saved));
 puts("Wi-Fi profile validation, merge, capacity, rotation and corrupt storage: PASS");
}
