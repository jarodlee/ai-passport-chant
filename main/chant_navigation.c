#include "chant_navigation.h"
#include "chant_model.h"
#include "chant_network.h"
static const struct { const char *title; chant_page_t page; } home_items[CHANT_HOME_ITEMS]={
    {"扫描Wi-Fi",PAGE_WIFI_SCAN}, {"二维码",PAGE_QR}, {"佛音曲库",PAGE_MUSIC},
    {"安庆天气",PAGE_WEATHER}, {"连接Wi-Fi",PAGE_NETWORK}, {"语音音量",PAGE_VOICE_VOLUME},
    {"Cindy对话",PAGE_CINDY}
};
const char *chant_home_title(unsigned selection) { return home_items[selection%CHANT_HOME_ITEMS].title; }
chant_action_t chant_navigate(chant_navigation_t *state, chant_key_t key) {
    chant_action_t action={.audio=-1,.network=-1};
    if (key==KEY_BACK) {
        *state=(chant_navigation_t){.page=PAGE_HOME};
        action.network=NET_CANCEL_PROVISION; return action;
    }
    if (state->page==PAGE_PLAYER) {
        action.audio=key==KEY_OK ? CHANT_TOGGLE : key==KEY_UP ? CHANT_UP : CHANT_DOWN;
        return action;
    }
    if (state->page==PAGE_QR) return action;
    if (state->page==PAGE_WIFI_SCAN && key==KEY_OK) {
        action.network=NET_SCAN; return action;
    }
    if (state->page==PAGE_WIFI_LIST && key==KEY_OK) {
        action.network=NET_CONNECT_PROFILE; action.profile=(int)state->selection;
        state->page=PAGE_NETWORK; state->selection=0;
        return action;
    }
    if (state->page==PAGE_VOICE_VOLUME) {
        action.audio=key==KEY_OK ? CHANT_VOICE_TEST : key==KEY_UP ? CHANT_VOICE_UP : CHANT_VOICE_DOWN;
        return action;
    }
    if (key!=KEY_OK) {
        unsigned count=state->page==PAGE_HOME ? CHANT_HOME_ITEMS : state->page==PAGE_MUSIC ? 6 : state->page==PAGE_NETWORK ? 4 :
                       state->page==PAGE_WIFI_LIST ? (state->items ? state->items : 1) : 1;
        state->selection=(state->selection+count+(key==KEY_UP ? -1 : 1))%count;
        state->confirm_forget=false; return action;
    }
    if (state->page==PAGE_HOME) {
        state->page=home_items[state->selection%CHANT_HOME_ITEMS].page; state->selection=0;
        if (state->page==PAGE_WIFI_SCAN) action.network=NET_SCAN;
        /* Every entry refreshes once; the worker ignores it while offline. */
        if (state->page==PAGE_WEATHER) action.network=NET_REFRESH_WEATHER;
    } else if (state->page==PAGE_MUSIC) {
        if (state->selection==5) action.audio=CHANT_SEQUENCE;
        else { action.audio=CHANT_SELECT_0+state->selection; state->page=PAGE_PLAYER; }
    } else if (state->page==PAGE_WEATHER) action.network=NET_REFRESH_WEATHER;
    else if (state->page==PAGE_NETWORK) {
        if (state->selection==0) action.network=NET_PROVISION;
        else if (state->selection==1) { state->page=PAGE_WIFI_LIST; state->selection=0; }
        else if (state->selection==2) { state->page=PAGE_WIFI_SCAN; action.network=NET_SCAN; }
        else if (state->confirm_forget) { action.network=NET_FORGET; state->confirm_forget=false; }
        else state->confirm_forget=true;
    }
    return action;
}
