#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "chant_wifi_profiles.h"
#define CHANT_SCAN_MAX 8
typedef struct { char ssid[33]; int8_t rssi; uint8_t channel; bool secure; } chant_scan_ap_t;
typedef enum { NET_STARTING, NET_OFFLINE, NET_CONNECTING, NET_CONNECTED, NET_BAD_WIFI, NET_FAILED } chant_net_status_t;
typedef struct {
    chant_net_status_t status;
    bool provisioning, weather_valid, weather_error, weather_refreshing, save_error, scanning, scan_error;
    char ap_name[24], ap_password[13], ip[16], weather_time[20], ssid[33];
    uint8_t profile_count, active_profile, scan_count;
    chant_scan_ap_t scan[CHANT_SCAN_MAX];
    char profile_ssid[CHANT_WIFI_MAX_PROFILES][33];
    float temperature;
    unsigned humidity;
    int weather_code;
    int64_t weather_updated_us, weather_fetched_us, weather_observed_at;
} chant_network_state_t;
typedef enum { NET_PROVISION, NET_CANCEL_PROVISION, NET_REFRESH_WEATHER, NET_FORGET, NET_CONNECT_PROFILE, NET_SCAN } chant_net_command_t;
bool chant_network_start(void);
bool chant_network_command(chant_net_command_t command);
bool chant_network_connect_profile(uint8_t index);
void chant_network_snapshot(chant_network_state_t *state);
