#include "chant_navigation.h"
#include "chant_model.h"
#include "chant_network.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    chant_navigation_t n={0}; chant_action_t a;
    chant_navigate(&n,KEY_UP); assert(n.selection==4);
    chant_navigate(&n,KEY_OK); assert(n.page==PAGE_VOICE_VOLUME);
    a=chant_navigate(&n,KEY_UP); assert(a.audio==CHANT_VOICE_UP);
    a=chant_navigate(&n,KEY_DOWN); assert(a.audio==CHANT_VOICE_DOWN);
    a=chant_navigate(&n,KEY_OK); assert(a.audio==CHANT_VOICE_TEST);
    a=chant_navigate(&n,KEY_BACK); assert(n.page==PAGE_HOME);
    chant_navigate(&n,KEY_DOWN); chant_navigate(&n,KEY_DOWN); chant_navigate(&n,KEY_DOWN); chant_navigate(&n,KEY_OK); assert(n.page==PAGE_CINDY);
    a=chant_navigate(&n,KEY_OK); assert(a.audio==-1 && a.network==-1);
    a=chant_navigate(&n,KEY_BACK); assert(n.page==PAGE_HOME && a.network==NET_CANCEL_PROVISION);
    chant_navigate(&n,KEY_OK); assert(n.page==PAGE_MUSIC);
    chant_navigate(&n,KEY_UP); a=chant_navigate(&n,KEY_OK);
    assert(a.audio==CHANT_SEQUENCE && n.page==PAGE_MUSIC);
    chant_navigate(&n,KEY_UP); a=chant_navigate(&n,KEY_OK);
    assert(a.audio==CHANT_SELECT_4 && n.page==PAGE_PLAYER);
    a=chant_navigate(&n,KEY_UP); assert(a.audio==CHANT_UP);
    a=chant_navigate(&n,KEY_OK); assert(a.audio==CHANT_TOGGLE);
    chant_navigate(&n,KEY_BACK);
    chant_navigate(&n,KEY_DOWN); a=chant_navigate(&n,KEY_OK);
    assert(n.page==PAGE_WEATHER && a.network==NET_REFRESH_WEATHER);
    a=chant_navigate(&n,KEY_OK); assert(a.network==NET_REFRESH_WEATHER);
    chant_navigate(&n,KEY_BACK);
    chant_navigate(&n,KEY_DOWN); chant_navigate(&n,KEY_DOWN); chant_navigate(&n,KEY_OK);
    assert(n.page==PAGE_NETWORK);
    a=chant_navigate(&n,KEY_OK); assert(a.network==NET_PROVISION);
    chant_navigate(&n,KEY_UP); a=chant_navigate(&n,KEY_OK);
    assert(n.confirm_forget && a.network==-1);
    chant_navigate(&n,KEY_DOWN); assert(!n.confirm_forget);
    chant_navigate(&n,KEY_UP); chant_navigate(&n,KEY_OK);
    a=chant_navigate(&n,KEY_OK); assert(a.network==NET_FORGET && !n.confirm_forget);
    n.selection=0;
    chant_navigate(&n,KEY_DOWN);
    a=chant_navigate(&n,KEY_OK); assert(n.page==PAGE_WIFI_LIST && a.network==-1);
    n.items=3;
    a=chant_navigate(&n,KEY_DOWN); assert(n.selection==1 && a.network==-1);
    a=chant_navigate(&n,KEY_OK);
    assert(a.network==NET_CONNECT_PROFILE && a.profile==1 && n.page==PAGE_NETWORK);
    n.selection=0;
    chant_navigate(&n,KEY_DOWN); chant_navigate(&n,KEY_DOWN);
    a=chant_navigate(&n,KEY_OK);
    assert(n.page==PAGE_WIFI_SCAN && a.network==NET_SCAN);
    a=chant_navigate(&n,KEY_OK); assert(a.network==NET_SCAN);
    puts("Chant page navigation and forget confirmation: PASS");
}
