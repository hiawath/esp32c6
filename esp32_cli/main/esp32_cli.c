#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "led_control.h"
#include "cli_console.h"

// ESP32-C6-DevKitC-1 온보드 RGB LED(WS2812) GPIO 번호
#define ONBOARD_LED_GPIO_NUM 8

static const char *TAG = "main";

void app_main(void)
{
    // NVS 초기화 (console 및 wifi 등의 내부 라이브러리 사용 시 필요할 수 있음)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting ESP32-C6 CLI Project...");

    // 1. LED 하드웨어 초기화 (GPIO 8)
    ret = led_control_init(ONBOARD_LED_GPIO_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED control: %s", esp_err_to_name(ret));
        return;
    }

    // 2. CLI 콘솔 시작
    ret = cli_console_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start CLI console: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Initialization complete.");
}
