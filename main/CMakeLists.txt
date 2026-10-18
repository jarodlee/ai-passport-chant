idf_component_register(
    SRCS "chant_app.c" "chant_voice.c" "chant_adpcm.c" "chant_model.c" "chant_playlist.c" "chant_network.c" "chant_wifi_profiles.c" "chant_services_model.c" "chant_storage.c" "chant_navigation.c" "chant_settings.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_wifi esp_netif esp_event nvs_flash esp_http_server esp_http_client mbedtls json lwip
    EMBED_FILES "../assets/music/dabei.adpcm" "../assets/music/guanyin.adpcm" "../assets/music/auspicious.adpcm" "../assets/music/xinjing.adpcm" "../assets/music/xinjinglong.adpcm"
)
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/chant_font_18.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/chant_font_28.c")
