#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CHANT_WIFI_MAX_PROFILES 6u
#define CHANT_WIFI_PROFILE_VERSION 1u
typedef struct { char ssid[33], password[64]; } chant_wifi_profile_t;
typedef struct {
    uint32_t version, count, active;
    chant_wifi_profile_t entries[CHANT_WIFI_MAX_PROFILES];
} chant_wifi_profiles_t;
void chant_wifi_profiles_init(chant_wifi_profiles_t *profiles);
bool chant_wifi_profiles_valid(const chant_wifi_profiles_t *profiles);
/* Update matching SSID or append; never evict a saved network silently. */
int chant_wifi_profiles_put(chant_wifi_profiles_t *profiles,const char *ssid,const char *password);
void chant_wifi_profiles_next(chant_wifi_profiles_t *profiles);
