#pragma once
#include <stdbool.h>
#define CHANT_VOICE_BASE_SIZE 192
/* Only HTTPS DNS origins are accepted; no credentials, paths or redirects. */
bool chant_voice_remote_base(const char *configured, char base[CHANT_VOICE_BASE_SIZE]);
