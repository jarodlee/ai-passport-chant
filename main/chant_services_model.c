#include "chant_services_model.h"
#include <string.h>
#include <math.h>

bool chant_wifi_credentials_valid(const char *ssid, const char *password) {
    if (!ssid || !password) return false;
    size_t n = strlen(ssid), p = strlen(password);
    if (n == 0 || n > 32 || p > 63 || (p > 0 && p < 8)) return false;
    /* WPA passphrases are printable ASCII. SSID bytes may be UTF-8. */
    for (size_t i = 0; i < p; i++) if ((unsigned char)password[i] < 32 || (unsigned char)password[i] > 126) return false;
    return true;
}
const char *chant_weather_description(int code) {
    switch (code) {
        case 0: return "晴";
        case 1: case 2: return "多云";
        case 3: return "阴";
        case 45: case 48: return "雾";
        case 51: case 53: case 55: return "毛毛雨";
        case 56: case 57: case 66: case 67: return "冻雨";
        case 61: case 63: case 65: return "雨";
        case 71: case 73: case 75: case 77: return "雪";
        case 80: case 81: case 82: return "阵雨";
        case 85: case 86: return "阵雪";
        case 95: case 96: case 99: return "雷雨";
        default: return "天气未知";
    }
}
bool chant_weather_values_valid(double temperature, double humidity, double code) {
    if (!isfinite(temperature) || !isfinite(humidity) || !isfinite(code)) return false;
    if (temperature < -80 || temperature > 65 || humidity < 0 || humidity > 100) return false;
    return code >= 0 && code <= 99 && code == (int)code && strcmp(chant_weather_description((int)code), "天气未知") != 0;
}
bool chant_weather_stale(int64_t updated_us, int64_t now_us) {
    return updated_us <= 0 || now_us < updated_us || now_us - updated_us > INT64_C(1800000000);
}
