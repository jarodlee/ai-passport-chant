#include "chant_voice_endpoint.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    char base[CHANT_VOICE_BASE_SIZE];
    const char *bad[] = {NULL, "", "http://voice.example.com", "https://",
        "https://localhost", "https://127.0.0.1", "https://voice.example.com:80",
        "https://user@voice.example.com", "https://voice.example.com/path",
        "https://voice.example.com?x=1", "https://voice.example.com#x",
        "https://voice.example.com\\@evil.test", "https://voice..example.com",
        "https://-voice.example.com", "https://voice-.example.com",
        "https://voice.example.com.", "https://voice.example.com//",
        "https://voice.example.com\r\nX-Test: value"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        memset(base, 'x', sizeof(base));
        assert(!chant_voice_remote_base(bad[i], base));
        assert(!base[0]);
    }
    assert(chant_voice_remote_base("https://voice.example.com/", base));
    assert(!strcmp(base, "https://voice.example.com"));
    assert(chant_voice_remote_base("https://voice-1.example.com", base));
    char long_url[300];
    memset(long_url, 'a', sizeof(long_url));
    memcpy(long_url, "https://", 8);
    memcpy(long_url + 72, ".example.com", 13);
    assert(!chant_voice_remote_base(long_url, base)); /* 64-byte label */
    memset(long_url + 8, 'a', sizeof(long_url) - 9);
    long_url[sizeof(long_url) - 1] = 0;
    assert(!chant_voice_remote_base(long_url, base)); /* bounded origin */
    puts("Voice HTTPS origin validation: PASS");
    return 0;
}
