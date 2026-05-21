idf_component_register(SRCS "src/led_control.c" "src/wifi_app.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver esp_wifi esp_event)
