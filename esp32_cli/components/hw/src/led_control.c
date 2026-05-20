#include "led_control.h"
#include "ws2812_driver.h"
#include "esp_log.h"

static const char *TAG = "led_control";

typedef struct {
    bool is_on;
    uint8_t brightness;
} channel_state_t;

// RED, GREEN, BLUE, WHITE 채널 상태 저장 (초기값: OFF, 기본 밝기 128)
static channel_state_t s_channels[LED_CHANNEL_WHITE + 1] = {
    [LED_CHANNEL_RED]   = { .is_on = false, .brightness = 128 },
    [LED_CHANNEL_GREEN] = { .is_on = false, .brightness = 128 },
    [LED_CHANNEL_BLUE]  = { .is_on = false, .brightness = 128 },
    [LED_CHANNEL_WHITE] = { .is_on = false, .brightness = 128 },
};

esp_err_t led_control_init(int gpio_num)
{
    // 온보드 LED 개수는 1개로 가정합니다.
    esp_err_t err = ws2812_driver_init(gpio_num, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WS2812 driver: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "LED Control initialized successfully on GPIO %d", gpio_num);
    return ESP_OK;
}

esp_err_t led_control_set_channel(led_channel_t channel, bool is_on, uint8_t brightness)
{
    if (channel < 0 || channel >= LED_CHANNEL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (channel == LED_CHANNEL_ALL) {
        for (int i = 0; i <= LED_CHANNEL_WHITE; i++) {
            s_channels[i].is_on = is_on;
            if (brightness > 0) {
                s_channels[i].brightness = brightness;
            }
        }
        ESP_LOGI(TAG, "Set ALL channels: %s, brightness: %d", is_on ? "ON" : "OFF", brightness);
    } else {
        s_channels[channel].is_on = is_on;
        if (brightness > 0) {
            s_channels[channel].brightness = brightness;
        }
        ESP_LOGI(TAG, "Set channel %d: %s, brightness: %d", channel, is_on ? "ON" : "OFF", s_channels[channel].brightness);
    }

    return ESP_OK;
}

esp_err_t led_control_apply(void)
{
    uint8_t r = s_channels[LED_CHANNEL_RED].is_on ? s_channels[LED_CHANNEL_RED].brightness : 0;
    uint8_t g = s_channels[LED_CHANNEL_GREEN].is_on ? s_channels[LED_CHANNEL_GREEN].brightness : 0;
    uint8_t b = s_channels[LED_CHANNEL_BLUE].is_on ? s_channels[LED_CHANNEL_BLUE].brightness : 0;
    uint8_t w = s_channels[LED_CHANNEL_WHITE].is_on ? s_channels[LED_CHANNEL_WHITE].brightness : 0;

    // 첫 번째 LED (index 0)로 값을 전송
    esp_err_t err = ws2812_driver_set_pixel(0, r, g, b, w);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pixel color");
        return err;
    }

    err = ws2812_driver_refresh();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip");
        return err;
    }

    return ESP_OK;
}

bool led_control_get_channel_state(led_channel_t channel, uint8_t *out_brightness)
{
    if (channel < 0 || channel > LED_CHANNEL_WHITE) {
        return false;
    }
    if (out_brightness != NULL) {
        *out_brightness = s_channels[channel].brightness;
    }
    return s_channels[channel].is_on;
}
