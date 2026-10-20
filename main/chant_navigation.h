#pragma once
#include <stdbool.h>
typedef enum { PAGE_HOME, PAGE_MUSIC, PAGE_PLAYER, PAGE_WEATHER, PAGE_NETWORK, PAGE_CINDY, PAGE_VOICE_VOLUME, PAGE_WIFI_LIST, PAGE_WIFI_SCAN, PAGE_QR } chant_page_t;
enum { CHANT_HOME_ITEMS = 7 };
const char *chant_home_title(unsigned selection);
typedef enum { KEY_UP, KEY_DOWN, KEY_OK, KEY_BACK } chant_key_t;
typedef struct { chant_page_t page; unsigned selection, items; bool confirm_forget; } chant_navigation_t;
typedef struct { int audio, network, profile; } chant_action_t;
/* Pure state machine. -1 means no operation; platform dispatch remains in app. */
chant_action_t chant_navigate(chant_navigation_t *state, chant_key_t key);
