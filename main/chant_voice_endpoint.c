#include "chant_voice_endpoint.h"
#include <string.h>

static bool alnum_ascii(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}
bool chant_voice_remote_base(const char *configured, char base[CHANT_VOICE_BASE_SIZE]) {
    base[0] = 0;
    if (!configured || strncmp(configured, "https://", 8)) return false;
    size_t length = strlen(configured);
    if (length && configured[length - 1] == '/') length--;
    if (length <= 8 || length >= CHANT_VOICE_BASE_SIZE) return false;
    size_t label = 0;
    bool dotted = false, letter = false;
    for (size_t i = 8; i < length; i++) {
        unsigned char c = (unsigned char)configured[i];
        if (c == '.') {
            if (!label || configured[i - 1] == '-') return false;
            label = 0; dotted = true;
        } else {
            if ((!alnum_ascii(c) && c != '-') || (!label && c == '-') || ++label > 63)
                return false;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) letter = true;
        }
    }
    if (!label || configured[length - 1] == '-' || !dotted || !letter) return false;
    memcpy(base, configured, length);
    base[length] = 0;
    return true;
}
