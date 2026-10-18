#include "chant_adpcm.h"
#include "chant_voice.h"
#include "chant_model.h"
#include "chant_navigation.h"
#include "chant_playlist.h"
#include "chant_network.h"
#include "chant_storage.h"
#include "chant_settings.h"
#include "chant_services_model.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(chant_font_18);
LV_FONT_DECLARE(chant_font_28);
static const char *TAG = "chant_player";
static QueueHandle_t commands, snapshots;
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } key_event_t;
static QueueHandle_t keys;
static lv_obj_t *heading, *body, *hint, *battery_label, *menu_label, *reply_canvas, *top_label;
static unsigned shown_reply_serial;
static bool canvas_ready;
static uint8_t initial_voice_volume = 80;
static chant_navigation_t navigation;
/* Fixed strings from all app modules are included by the generator and runtime probe. */
#include "chant_font_inventory.h"


static void on_button(bsp_btn_t btn, bsp_btn_ev_t event, void *user) {
    (void)user;
    if (event != BSP_BTN_CLICK && event != BSP_BTN_DOUBLE && event != BSP_BTN_LONG && event != BSP_BTN_PRESS && event != BSP_BTN_RELEASE) return;
    key_event_t key = {.button=btn, .event=event};
    if (keys) (void)xQueueSend(keys, &key, 0);
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int y, const lv_font_t *font) {
    lv_obj_t *obj=lv_label_create(parent);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(0xEFE6D2),0);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_width(obj,212);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_WRAP);
    lv_label_set_text(obj,text);
    lv_obj_align(obj,LV_ALIGN_TOP_MID,0,y);
    return obj;
}
static bool check_font(const lv_font_t *font, const char *p) {
    while (*p) {
        uint32_t cp=(uint8_t)*p++;
        if (cp >= 0xE0) {
            cp=((cp & 15)<<12) | (((uint8_t)p[0]&63)<<6) | ((uint8_t)p[1]&63); p+=2;
        }
        lv_font_glyph_dsc_t glyph={0};
        if (!lv_font_get_glyph_dsc(font,&glyph,cp,0) || glyph.is_placeholder) {
            ESP_LOGE(TAG,"Missing glyph U+%04lx",(unsigned long)cp); return false;
        }
    }
    lv_font_glyph_dsc_t negative={0};
    return !lv_font_get_glyph_dsc(font,&negative,0x9F98,0) || negative.is_placeholder;
}
static void build_screen(void) {
    lv_obj_t *screen=lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x162A26),0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    top_label=lv_label_create(screen);
    lv_obj_set_style_text_font(top_label,&chant_font_18,0);
    lv_obj_set_style_text_color(top_label,lv_color_hex(0xEFE6D2),0);
    lv_obj_set_style_text_align(top_label,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_width(top_label,168);
    lv_label_set_long_mode(top_label,LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(top_label,8,4);
    lv_obj_add_flag(top_label,LV_OBJ_FLAG_HIDDEN);
    heading=label(screen,"安心听",52,&chant_font_28);
    body=label(screen,"准备中",90,&chant_font_18);
    lv_obj_set_height(body,186);
    menu_label=label(screen,"",186,&chant_font_28);
    lv_obj_set_width(menu_label,212);
    lv_obj_add_flag(menu_label,LV_OBJ_FLAG_HIDDEN);
    reply_canvas=lv_canvas_create(screen);
    lv_obj_set_size(reply_canvas,CHANT_REPLY_WIDTH,CHANT_REPLY_HEIGHT);
    lv_obj_align(reply_canvas,LV_ALIGN_TOP_MID,0,90);
    lv_obj_set_style_border_width(reply_canvas,1,0);
    lv_obj_set_style_border_color(reply_canvas,lv_color_hex(0x35564D),0);
    lv_obj_set_style_border_opa(reply_canvas,LV_OPA_COVER,0);
    lv_obj_set_style_pad_all(reply_canvas,0,0);
    lv_obj_add_flag(reply_canvas,LV_OBJ_FLAG_HIDDEN);
    hint=label(screen,"上下选择 确定进入",276,&chant_font_18);
    battery_label=label(screen,"--%",8,&chant_font_18);
    lv_obj_set_width(battery_label,52);
    lv_obj_align(battery_label,LV_ALIGN_TOP_RIGHT,-8,8);
    lv_screen_load(screen);
}
static const char *net_text(chant_net_status_t status) {
    switch (status) {
        case NET_STARTING: return "网络准备中";
        case NET_CONNECTING: return "连接中";
        case NET_CONNECTED: return "Wi-Fi已连接";
        case NET_BAD_WIFI: return "连接失败 请重新配网";
        case NET_FAILED: return "网络不可用";
        default: return "Wi-Fi未连接";
    }
}
static void send_audio(chant_command_t command) { (void)xQueueSend(commands,&command,0); }
/* Staleness is wall-clock based; uptime alone cannot age an observation made before boot. */
static void display_ssid(char *out, size_t size, const char *ssid) {
    size_t j=0;
    for (size_t i=0; ssid && ssid[i] && i<32 && j+1<size; i++) {
        unsigned char c=(unsigned char)ssid[i];
        out[j++]=(c>=32 && c<127)?(char)c:'?';
    }
    out[j]=0;
}
static bool weather_stale(const chant_network_state_t *net) {
    if (!net->weather_valid || net->weather_observed_at <= 0) return false;
    time_t now=time(NULL);
    return now < net->weather_observed_at || now - net->weather_observed_at > 1800;
}
static void handle_key(key_event_t key) {
    /* A double click never emits single clicks, so it is free for a quick return home. */
    if (key.event==BSP_BTN_DOUBLE) {
        if (key.button==BSP_BTN_UP) {
            if (chant_voice_busy()) chant_voice_cancel();
            chant_page_t before=navigation.page;
            (void)chant_navigate(&navigation,KEY_BACK);
            if (before==PAGE_CINDY) chant_voice_clear_reply();
        }
        return;
    }
    if (navigation.page==PAGE_CINDY) {
        if (key.button==BSP_BTN_OK && (key.event==BSP_BTN_PRESS || key.event==BSP_BTN_RELEASE)) chant_voice_press(key.event==BSP_BTN_PRESS);
        if (key.button==BSP_BTN_UP && key.event==BSP_BTN_CLICK) {
            /* Leaving during a reply stops playback instead of trapping the user. */
            if (chant_voice_busy()) chant_voice_cancel();
            (void)chant_navigate(&navigation,KEY_BACK); chant_voice_clear_reply();
        }
        return;
    }
    if (key.event==BSP_BTN_PRESS || key.event==BSP_BTN_RELEASE) return;
    if (key.event==BSP_BTN_LONG && key.button!=BSP_BTN_OK) return;
    chant_key_t input=key.event==BSP_BTN_LONG ? KEY_BACK : key.button==BSP_BTN_OK ? KEY_OK : key.button==BSP_BTN_UP ? KEY_UP : KEY_DOWN;
    chant_page_t before=navigation.page;
    if (navigation.page==PAGE_WIFI_LIST) {
        chant_network_state_t network; chant_network_snapshot(&network);
        navigation.items=network.profile_count?network.profile_count:1;
        if (navigation.selection>=navigation.items) navigation.selection=navigation.items-1;
    }
    if (navigation.page==PAGE_NETWORK && input!=KEY_BACK) {
        chant_network_state_t network; chant_network_snapshot(&network);
        if (network.provisioning) {
            if (input==KEY_OK) (void)chant_network_command(NET_CANCEL_PROVISION);
            return;
        }
    }
    chant_action_t action=chant_navigate(&navigation,input);
    if (before==PAGE_CINDY && navigation.page!=PAGE_CINDY) chant_voice_clear_reply();
    if (navigation.page==PAGE_WIFI_LIST && before!=PAGE_WIFI_LIST) {
        chant_network_state_t network; chant_network_snapshot(&network);
        navigation.selection=network.active_profile<network.profile_count?network.active_profile:0;
    }
    if (action.audio>=0) send_audio((chant_command_t)action.audio);
    if (action.network==NET_CONNECT_PROFILE) {
        chant_network_state_t network; chant_network_snapshot(&network);
        if (action.profile>=0 && (unsigned)action.profile<network.profile_count)
            (void)chant_network_connect_profile((uint8_t)action.profile);
    } else if (action.network>=0) (void)chant_network_command((chant_net_command_t)action.network);
}
static void render(const chant_model_t *audio, const chant_network_state_t *net, int battery) {
    const char *title="安心听", *footer="长按确定返回主页";
    const char *playing=audio->failed ? "音频不可用" : audio->playing ? "正在播放" : "已暂停";
    char text[640], weather[96];
    bool stale=weather_stale(net);
    if (net->weather_valid) snprintf(weather,sizeof(weather),"安庆 %.1f C %s%s",(double)net->temperature,
        chant_weather_description(net->weather_code), stale ? " 已过期" : "");
    else snprintf(weather,sizeof(weather),"安庆天气 %s",net->status==NET_CONNECTED ? "等待更新" : "联网后显示");
    unsigned selection=navigation.selection;
    bool confirm_forget=navigation.confirm_forget;
    const char *menu=NULL;
    char menu_text[96];
    bool show_reply=false;
    const uint8_t *reply_pixels=NULL;
    unsigned reply_serial=0;
    const char *top_text=NULL;
    char top_buf[64];
    switch (navigation.page) {
        case PAGE_HOME: {
            const char *items[]={"佛音曲库","安庆天气","连接Wi-Fi","Cindy对话","语音音量"};
            snprintf(text,sizeof(text),"%s\n%s\n%s\nNFC: 手机靠近识别",weather,
                chant_tracks[audio->track].title,playing);
            snprintf(menu_text,sizeof(menu_text),"▶ %s ◀",items[selection]);
            menu=menu_text;
            char ssid[15], when[24]="--";
            display_ssid(ssid,sizeof(ssid),net->ssid);
            time_t now=time(NULL);
            if (now>1700000000) { struct tm local; localtime_r(&now,&local); strftime(when,sizeof(when),"%Y-%m-%d %H:%M",&local); }
            snprintf(top_buf,sizeof(top_buf),"%s\n%s",
                net->status==NET_CONNECTED && ssid[0] ? ssid : "Wi-Fi未连接", when);
            top_text=top_buf;
            footer="上下选择 确定进入"; break;
        }
        case PAGE_MUSIC:
            title="佛音曲库";
            snprintf(text,sizeof(text),"%s 大悲咒\n%s 观音菩萨圣号\n%s 消灾吉祥神咒\n%s 心经\n%s 心经长版\n\n%s %s",
                selection==0?">":" ",selection==1?">":" ",selection==2?">":" ",selection==3?">":" ",
                selection==4?">":" ",selection==5?">":" ", audio->sequence ? "顺序循环" : "单曲循环");
            break;
        case PAGE_PLAYER: {
            title=chant_tracks[audio->track].title;
            unsigned second=audio->position/CHANT_AUDIO_RATE, total=chant_tracks[audio->track].samples/CHANT_AUDIO_RATE;
            snprintf(text,sizeof(text),"万佛城唱诵 / 离线\n\n%s\n%u:%02u / %u:%02u\n音量 %u%% / %s\n长按确定返回主页",playing,
                second/60,second%60,total/60,total%60,audio->volume,audio->sequence?"顺序循环":"单曲循环");
            footer="上下调音量 确定暂停"; break;
        }
        case PAGE_WEATHER:
            title="安庆天气";
            if (net->weather_valid) {
                char fresh[32]="每次进入自动刷新";
                if (net->weather_refreshing) snprintf(fresh,sizeof(fresh),"正在刷新");
                else if (net->weather_error) snprintf(fresh,sizeof(fresh),"刷新失败 保留上次数据");
                else if (stale) snprintf(fresh,sizeof(fresh),"数据已过期");
                else if (net->weather_fetched_us) {
                    int64_t age=(esp_timer_get_time()-net->weather_fetched_us)/1000000;
                    if (age<60) snprintf(fresh,sizeof(fresh),"刚刚刷新");
                    else if (age<3600) snprintf(fresh,sizeof(fresh),"%lld 分钟前刷新",(long long)(age/60));
                    else snprintf(fresh,sizeof(fresh),"%lld 小时前刷新",(long long)(age/3600));
                }
                snprintf(text,sizeof(text),"%.1f C  %s\n湿度 %u%%\n\n观测 %s\n%s\nOpen-Meteo",
                    (double)net->temperature,chant_weather_description(net->weather_code),net->humidity,net->weather_time,fresh);
            } else snprintf(text,sizeof(text),"%s\n\n%s\n\n确定刷新天气\nOpen-Meteo",net_text(net->status),
                net->weather_refreshing?"正在刷新天气":net->weather_error?"获取失败 稍后重试":net->status==NET_CONNECTED?"正在刷新天气":"联网后自动获取");
            break;
        case PAGE_NETWORK: {
            title="连接Wi-Fi";
            if (net->provisioning) footer="确定关闭 长按返回";
            if (net->provisioning) snprintf(text,sizeof(text),"手机连接设备热点\n%s\n无需密码 直接连接\n浏览器打开\nhttp://192.168.4.1\n%s",net->ap_name,net_text(net->status));
            else {
                char ssid[24], head[64]; display_ssid(ssid,sizeof(ssid),net->ssid);
                if (net->status==NET_CONNECTED && ssid[0]) {
                    if (net->ip[0]) snprintf(head,sizeof(head),"已连 %s\n%s",ssid,net->ip);
                    else snprintf(head,sizeof(head),"已连 %s",ssid);
                } else snprintf(head,sizeof(head),"%s",net_text(net->status));
                snprintf(text,sizeof(text),"%s\n\n%s 手机配网\n%s 选择已存网络\n%s 扫描周边网络\n%s %s",
                    head, selection==0?">":" ", selection==1?">":" ", selection==2?">":" ", selection==3?">":" ",
                    confirm_forget?"再按确定清除":"忘记已存网络");
                footer = net->save_error ? "保存失败 请重试" : "仅支持2.4G 长按返回";
            }
            break;
        }
        case PAGE_WIFI_SCAN: {
            title="周边网络";
            if (net->scanning) snprintf(text,sizeof(text),"正在扫描...\n请稍候");
            else if (net->scan_error) snprintf(text,sizeof(text),"扫描失败\n\n确定重新扫描");
            else if (!net->scan_count) snprintf(text,sizeof(text),"未发现附近网络\n\n确定重新扫描");
            else {
                size_t used=0; text[0]=0;
                unsigned shown=net->scan_count<CHANT_SCAN_MAX?net->scan_count:CHANT_SCAN_MAX;
                for (unsigned i=0;i<shown && used<sizeof(text)-40;i++) {
                    char ssid[14]; display_ssid(ssid,sizeof(ssid),net->scan[i].ssid);
                    used += (size_t)snprintf(text+used,sizeof(text)-used,"%s %d %u%s\n",ssid,
                        (int)net->scan[i].rssi,(unsigned)net->scan[i].channel,net->scan[i].secure?"*":"");
                }
            }
            footer="dBm 信道 *加密 确定重扫";
            break;
        }
        case PAGE_WIFI_LIST: {
            title="已存网络";
            unsigned count=net->profile_count;
            if (!count) snprintf(text,sizeof(text),"没有已保存的网络\n\n长按确定返回主页");
            else {
                size_t used=0; text[0]=0;
                for (unsigned i=0;i<count && i<CHANT_WIFI_MAX_PROFILES && used<sizeof(text)-40;i++) {
                    char ssid[20]; display_ssid(ssid,sizeof(ssid),net->profile_ssid[i]);
                    used += (size_t)snprintf(text+used,sizeof(text)-used,"%s %s%s\n",
                        i==selection?">":" ", ssid, i==net->active_profile?" 已连":"");
                }
            }
            footer="上下选择 确定连接";
            break;
        }
        case PAGE_VOICE_VOLUME:
            title="语音音量";
            snprintf(text,sizeof(text),"Cindy回复音量\n\n▶ %u%% ◀\n\n上下每次十个百分点\n确定试听",audio->voice_volume);
            footer="上下调整 确定试听"; break;
        default: {
            title="Cindy对话";
            show_reply=chant_voice_reply(&reply_pixels,&reply_serial);
            snprintf(text,sizeof(text),"%s\n\n%s\nMac与设备需在同一网络",net_text(net->status),chant_voice_status());
            footer=show_reply ? "按住再问 上键返回" : "按住确定说话 上键返回";
            break;
        }
    }
    /* The bitmap belongs to the Cindy page only; every other page must show the label. */
    if (show_reply) {
        if (!canvas_ready || reply_serial!=shown_reply_serial) {
            lv_canvas_set_buffer(reply_canvas,(void*)reply_pixels,CHANT_REPLY_WIDTH,CHANT_REPLY_HEIGHT,LV_COLOR_FORMAT_I1);
            lv_canvas_set_palette(reply_canvas,0,lv_color32_make(0x16,0x2A,0x26,0xFF));
            lv_canvas_set_palette(reply_canvas,1,lv_color32_make(0xEF,0xE6,0xD2,0xFF));
            shown_reply_serial=reply_serial; canvas_ready=true;
            lv_obj_invalidate(reply_canvas);
        }
        lv_obj_remove_flag(reply_canvas,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(body,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(reply_canvas,LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(body,LV_OBJ_FLAG_HIDDEN);
    }
    if (top_text) {
        if (strcmp(lv_label_get_text(top_label),top_text)) lv_label_set_text(top_label,top_text);
        lv_obj_remove_flag(top_label,LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(top_label,LV_OBJ_FLAG_HIDDEN);
    if (menu) {
        if (strcmp(lv_label_get_text(menu_label),menu)) lv_label_set_text(menu_label,menu);
        lv_obj_remove_flag(menu_label,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(menu_label,LV_OBJ_FLAG_HIDDEN);
    }
    if (strcmp(lv_label_get_text(heading),title)) lv_label_set_text(heading,title);
    if (strcmp(lv_label_get_text(body),text)) {
        lv_label_set_text(body,text);
        if (navigation.page==PAGE_WEATHER) ESP_LOGI(TAG,"Weather UI refreshed");
    }
    if (strcmp(lv_label_get_text(hint),footer)) lv_label_set_text(hint,footer);
    if (battery<0) lv_label_set_text(battery_label,"--%");
    else lv_label_set_text_fmt(battery_label,"%d%%",battery);
}

static esp_err_t audio_open(uint8_t volume) {
    /* Saving credentials puts the codec to sleep; wake it before setting format. */
    esp_err_t err = bsp_audio_wake();
    if (err == ESP_OK) err = bsp_audio_init();
    if (err == ESP_OK) err = bsp_audio_set_format(CHANT_AUDIO_RATE, 16, 1);
    if (err == ESP_OK) bsp_audio_set_volume(volume);
    return err;
}

static void play_beep(uint8_t volume) {
    static const int16_t cycle[12]={0,5000,8660,10000,8660,5000,0,-5000,-8660,-10000,-8660,-5000};
    int16_t pcm[512];
    bsp_audio_set_volume(volume);
    for (int chunk=0; chunk<8; chunk++) {
        for (int i=0; i<512; i++) {
            int n=chunk*512+i, envelope=255;
            if (n<256) envelope=n; else if (n>3840) envelope=4096-n;
            if (envelope<0) envelope=0;
            pcm[i]=(int16_t)((int32_t)cycle[n%12]*envelope/255);
        }
        if (bsp_audio_write(pcm,sizeof(pcm))!=ESP_OK) break;
    }
}

static void audio_worker(void *arg) {
    (void)arg;
    chant_model_t state; chant_model_init(&state); state.voice_volume=initial_voice_volume;
    chant_decoder_t decoder = {0};
    int16_t pcm[512];
    unsigned silence_samples = 0;
    bool fade_in = true;
    for (unsigned i=0; i<CHANT_TRACK_COUNT; i++) if ((size_t)(chant_tracks[i].end - chant_tracks[i].start) != (chant_tracks[i].samples + 1u) / 2u) {
        ESP_LOGE(TAG, "Audio asset size mismatch"); state.failed = true;
    }
    if (!state.failed && audio_open(state.volume) != ESP_OK) state.failed = true;
    ESP_LOGI(TAG, "Autoplay ready: samples=%u rate=%u volume=%u failed=%d heap=%u largest=%u",
             chant_tracks[state.track].samples, CHANT_AUDIO_RATE, state.volume, state.failed,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    int64_t last_snapshot = 0, last_feed = 0;
    int64_t max_gap = 0;
    unsigned chunks = 0;
    for (;;) {
        if (chant_storage_requested()) {
            esp_err_t paused=bsp_audio_sleep();
            chant_storage_quiet(paused==ESP_OK);
            chant_storage_wait_done();
            state.failed=audio_open(state.volume)!=ESP_OK;
            fade_in=true; last_feed=0; last_snapshot=0;
        }
        if (chant_voice_requested()) {
            chant_voice_run(state.voice_volume);
            state.failed=audio_open(state.volume)!=ESP_OK;
            fade_in=true; last_feed=0; last_snapshot=0;
        }
        chant_command_t command;
        while (xQueueReceive(commands, &command, 0) == pdTRUE) {
            bool was_playing = state.playing;
            chant_model_command(&state, command);
            if (command == CHANT_RESTART || (command >= CHANT_SELECT_0 && command <= CHANT_SELECT_4)) {
                decoder = (chant_decoder_t){0}; silence_samples = 0; fade_in = true;
                state.failed = audio_open(state.volume) != ESP_OK;
            } else if (command == CHANT_UP || command == CHANT_DOWN) {
                if (!state.failed) bsp_audio_set_volume(state.volume);
            } else if (command == CHANT_VOICE_UP || command == CHANT_VOICE_DOWN) {
                (void)chant_settings_set_voice_volume(state.voice_volume);
            } else if (command == CHANT_VOICE_TEST && !state.failed) {
                play_beep(state.voice_volume);
                bsp_audio_set_volume(state.volume);
            }
            if (!was_playing && state.playing) fade_in = true;
            ESP_LOGI(TAG, "Control command=%d playing=%d volume=%u", command, state.playing, state.volume);
            last_snapshot = 0;
        }
        int64_t now = esp_timer_get_time();
        if (now - last_snapshot >= 250000 || last_snapshot == 0) {
            (void)xQueueOverwrite(snapshots, &state); last_snapshot = now;
        }
        if (state.failed) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        size_t count = 512;
        bool advance = state.playing && silence_samples == 0;
        if (advance) {
            uint32_t remaining = chant_tracks[state.track].samples - state.position;
            if (count > remaining) count = remaining;
            chant_decode(&decoder, chant_tracks[state.track].start, state.position, count, pcm);
            if (fade_in) {
                for (size_t i = 0; i < 128 && i < count; i++) pcm[i] = (int16_t)((int32_t)pcm[i] * (int)i / 128);
                fade_in = false;
            }
        } else {
            memset(pcm, 0, sizeof(pcm));
            if (state.playing && silence_samples) {
                if (count > silence_samples) count = silence_samples;
                silence_samples -= count;
            }
        }
        now = esp_timer_get_time();
        if (last_feed && now - last_feed > max_gap) max_gap = now - last_feed;
        last_feed = now;
        if (bsp_audio_write(pcm, count * sizeof(int16_t)) != ESP_OK) {
            ESP_LOGE(TAG, "PCM write failed"); state.failed = true; last_snapshot = 0; continue;
        }
        if (advance && chant_model_advance(&state, count, chant_tracks[state.track].samples)) {
            decoder = (chant_decoder_t){0}; silence_samples = CHANT_AUDIO_RATE / 4; fade_in = true;
            ESP_LOGI(TAG, "Loop complete: %u", state.completed);
        }
        if (++chunks % 1000 == 0) {
            ESP_LOGI(TAG, "Audio feeding: position=%u max_gap_us=%lld heap=%u",
                     state.position, (long long)max_gap, (unsigned)esp_get_free_heap_size());
            max_gap = 0;
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "CHANT PLAYER v3: playlist, Wi-Fi, weather, push-to-talk Cindy");
    keys = xQueueCreate(12, sizeof(key_event_t));
    commands = xQueueCreate(16, sizeof(chant_command_t));
    snapshots = xQueueCreate(1, sizeof(chant_model_t));
    if (!commands || !snapshots || !keys || !chant_storage_init()) { ESP_LOGE(TAG, "Queue allocation failed"); return; }
    if (bsp_i2c_init() != ESP_OK) { ESP_LOGE(TAG, "I2C init failed"); return; }
    bool display = bsp_display_init() == ESP_OK && bsp_lvgl_init();
    if (display && bsp_lvgl_lock(1000)) {
        bool fonts = check_font(&chant_font_18,CHANT_UI_INVENTORY) && check_font(&chant_font_28,CHANT_TITLE_INVENTORY);
        ESP_LOGI(TAG, "Font coverage: %s", fonts ? "PASS" : "FAIL");
        if (fonts) build_screen(); else display = false;
        bsp_lvgl_unlock();
        bsp_display_backlight(70);
    } else display = false;
    bool battery = bsp_battery_init() == ESP_OK;
    esp_err_t buttons = bsp_button_init(on_button, NULL);
    if (buttons != ESP_OK) ESP_LOGE(TAG, "Button init failed: %s", esp_err_to_name(buttons));
    if (chant_settings_init()) initial_voice_volume=chant_settings_voice_volume();
    ESP_LOGI(TAG, "Voice volume: %u%%", initial_voice_volume);
    if (xTaskCreate(audio_worker, "chant_audio", 12288, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Audio task allocation failed");
        if (display && bsp_lvgl_lock(1000)) { lv_label_set_text(body, "音频不可用"); bsp_lvgl_unlock(); }
        return;
    }
    (void)chant_network_start();
    int battery_soc=battery ? bsp_battery_soc() : -1;
    int64_t next_battery=esp_timer_get_time()+30000000;
    int64_t last_key=esp_timer_get_time();
    bool dimmed=false;
    chant_model_t state; chant_model_init(&state);
    for (;;) {
        key_event_t key;
        while (xQueueReceive(keys,&key,0)==pdTRUE) {
            last_key=esp_timer_get_time();
            if (dimmed) { bsp_display_backlight(70); dimmed=false; }
            handle_key(key);
        }
        if (!dimmed && esp_timer_get_time()-last_key>60000000) { bsp_display_backlight(12); dimmed=true; }
        if (esp_timer_get_time()>=next_battery) {
            battery_soc=battery ? bsp_battery_soc() : -1;
            next_battery=esp_timer_get_time()+30000000;
        }
        (void)xQueueReceive(snapshots,&state,0);
        chant_network_state_t net; chant_network_snapshot(&net);
        if (display && bsp_lvgl_lock(100)) {
            render(&state,&net,battery_soc); bsp_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
