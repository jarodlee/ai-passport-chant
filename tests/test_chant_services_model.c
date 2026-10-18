#include "chant_services_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(chant_wifi_credentials_valid("办公室", ""));
    assert(chant_wifi_credentials_valid("Office", "abcd1234"));
    assert(!chant_wifi_credentials_valid("", "abcd1234"));
    assert(!chant_wifi_credentials_valid(NULL, "abcd1234"));
    assert(!chant_wifi_credentials_valid("Office", "short"));
    assert(!chant_wifi_credentials_valid("Office", "abcd\n1234"));
    assert(!chant_wifi_credentials_valid("Office", "中文密码abcd"));
    char ssid[34]; memset(ssid,'s',33); ssid[33]=0;
    assert(!chant_wifi_credentials_valid(ssid,""));
    ssid[32]=0; assert(chant_wifi_credentials_valid(ssid,""));
    assert(chant_weather_values_valid(24.5,80,0));
    assert(chant_weather_values_valid(-20,100,99));
    assert(!chant_weather_values_valid(NAN,50,0));
    assert(!chant_weather_values_valid(20,101,0));
    assert(!chant_weather_values_valid(20,50,100));
    assert(!chant_weather_values_valid(20,50,1.5));
    assert(!chant_weather_values_valid(20,50,4));
    assert(strcmp(chant_weather_description(99), "雷雨")==0);
    assert(strcmp(chant_weather_description(1234), "天气未知")==0);
    assert(chant_weather_stale(0,100));
    assert(!chant_weather_stale(1,1800000001));
    assert(chant_weather_stale(1,1800000002));
    assert(chant_weather_stale(100,50));
    puts("Wi-Fi input and weather validity/expiry: PASS");
}
