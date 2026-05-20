#include "ws2812_driver.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "ws2812_driver";
static led_strip_handle_t led_strip_handle = NULL;

esp_err_t ws2812_driver_init(int gpio_num, int led_count)
{
    ESP_LOGI(TAG, "Initializing LED strip on GPIO %d, count: %d", gpio_num, led_count);
    
    /* LED strip common configuration */
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = led_count,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // ESP32-C6 온보드 WS2812는 통상 GRB 포맷입니다.
        .led_model = LED_MODEL_WS2812,            // WS2812 모델 지정
        .flags.invert_out = false,
    };

    /* RMT specific configuration */
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    // led_strip 컴포넌트를 이용해 RMT 기반 LED 디바이스 생성
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip device: %s", esp_err_to_name(err));
        return err;
    }

    // 초기 상태로 전체 끔
    led_strip_clear(led_strip_handle);
    
    return ESP_OK;
}

esp_err_t ws2812_driver_set_pixel(int index, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
    if (led_strip_handle == NULL) {
        ESP_LOGE(TAG, "Driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 만약 온보드 LED가 RGB 3채널인데 White(W) 입력이 온 경우, 
    // 소프트웨어적으로 R, G, B 채널에 White 값을 합성하여 밝기를 올려줍니다.
    // (RGB 합성으로 흰색을 만들기 위함)
    int r = red + white;
    int g = green + white;
    int b = blue + white;
    
    // 255 초과 방지 클리핑
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return led_strip_set_pixel(led_strip_handle, index, r, g, b);
}

esp_err_t ws2812_driver_refresh(void)
{
    if (led_strip_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_refresh(led_strip_handle);
}

esp_err_t ws2812_driver_clear(void)
{
    if (led_strip_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_clear(led_strip_handle);
}
