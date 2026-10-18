#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool chant_wifi_credentials_valid(const char *ssid, const char *password);
const char *chant_weather_description(int code);
bool chant_weather_values_valid(double temperature, double humidity, double code);
bool chant_weather_stale(int64_t updated_us, int64_t now_us);
