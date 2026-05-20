#include "led_control.h"
#include "ws2812_driver.h"
#include "esp_log.h"

// FreeRTOS 헤더 추가
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "led_control";

typedef struct {
    bool is_on;                     // 소프트웨어 설정 상 켜짐 유무
    uint8_t brightness;             // 설정 밝기
    uint32_t blink_delay_ms;        // 깜빡임 딜레이 (0이면 깜빡이지 않고 고정)
    uint32_t last_toggle_tick;       // 마지막 토글 시점 (FreeRTOS 틱)
    bool current_physical_state;    // 실제 물리 LED의 점등 여부 (토글 상태)
} channel_state_t;

// RED, GREEN, BLUE, WHITE 채널 상태 저장 (초기값: OFF, 기본 밝기 128)
static channel_state_t s_channels[LED_CHANNEL_WHITE + 1] = {
    [LED_CHANNEL_RED]   = { .is_on = false, .brightness = 128, .blink_delay_ms = 0, .last_toggle_tick = 0, .current_physical_state = false },
    [LED_CHANNEL_GREEN] = { .is_on = false, .brightness = 128, .blink_delay_ms = 0, .last_toggle_tick = 0, .current_physical_state = false },
    [LED_CHANNEL_BLUE]  = { .is_on = false, .brightness = 128, .blink_delay_ms = 0, .last_toggle_tick = 0, .current_physical_state = false },
    [LED_CHANNEL_WHITE] = { .is_on = false, .brightness = 128, .blink_delay_ms = 0, .last_toggle_tick = 0, .current_physical_state = false },
};

// FreeRTOS 객체 핸들
static QueueHandle_t s_led_cmd_queue = NULL;
static SemaphoreHandle_t s_led_mutex = NULL;

/**
 * @brief LED 제어 및 깜빡임 연산을 담당하는 백그라운드 태스크
 */
static void led_control_task(void *pvParameters)
{
    led_cmd_msg_t msg;
    ESP_LOGI(TAG, "FreeRTOS LED Control (Blink Support) task started");

    while (1) {
        // 50ms 타임아웃 대기로 메시지 큐 수신 (명령이 없더라도 주기적으로 깨어남)
        if (xQueueReceive(s_led_cmd_queue, &msg, pdMS_TO_TICKS(50)) == pdTRUE) {
            
            // 공유 자원 수정을 위한 뮤텍스 획득
            if (xSemaphoreTake(s_led_mutex, portMAX_DELAY) == pdTRUE) {
                uint32_t current_tick = xTaskGetTickCount();

                if (msg.channel == LED_CHANNEL_ALL) {
                    for (int i = 0; i <= LED_CHANNEL_WHITE; i++) {
                        s_channels[i].is_on = msg.is_on;
                        if (msg.brightness > 0) {
                            s_channels[i].brightness = msg.brightness;
                        }
                        s_channels[i].blink_delay_ms = msg.blink_delay_ms;
                        s_channels[i].last_toggle_tick = current_tick;
                        s_channels[i].current_physical_state = msg.is_on;
                    }
                } else if (msg.channel < LED_CHANNEL_MAX) {
                    s_channels[msg.channel].is_on = msg.is_on;
                    if (msg.brightness > 0) {
                        s_channels[msg.channel].brightness = msg.brightness;
                    }
                    s_channels[msg.channel].blink_delay_ms = msg.blink_delay_ms;
                    s_channels[msg.channel].last_toggle_tick = current_tick;
                    s_channels[msg.channel].current_physical_state = msg.is_on;
                }

                xSemaphoreGive(s_led_mutex);
            }
        }

        // 모든 채널의 Blink 상태 검증 및 물리 토글 처리
        if (xSemaphoreTake(s_led_mutex, portMAX_DELAY) == pdTRUE) {
            uint32_t current_tick = xTaskGetTickCount();

            for (int i = 0; i <= LED_CHANNEL_WHITE; i++) {
                if (s_channels[i].is_on && s_channels[i].blink_delay_ms > 0) {
                    // 경과 틱 계산 및 시간차(ms) 도출
                    uint32_t diff_ticks = current_tick - s_channels[i].last_toggle_tick;
                    uint32_t diff_ms = diff_ticks * portTICK_PERIOD_MS;
                    
                    if (diff_ms >= s_channels[i].blink_delay_ms) {
                        // 실제 물리 전도 상태를 토글
                        s_channels[i].current_physical_state = !s_channels[i].current_physical_state;
                        s_channels[i].last_toggle_tick = current_tick;
                    }
                } else {
                    // 깜빡이지 않는 고정 모드이거나 꺼진 경우, 물리 상태와 논리 상태 동기화
                    s_channels[i].current_physical_state = s_channels[i].is_on;
                }
            }

            // RGBW 물리 조합 색상 결정
            uint8_t r = s_channels[LED_CHANNEL_RED].current_physical_state ? s_channels[LED_CHANNEL_RED].brightness : 0;
            uint8_t g = s_channels[LED_CHANNEL_GREEN].current_physical_state ? s_channels[LED_CHANNEL_GREEN].brightness : 0;
            uint8_t b = s_channels[LED_CHANNEL_BLUE].current_physical_state ? s_channels[LED_CHANNEL_BLUE].brightness : 0;
            uint8_t w = s_channels[LED_CHANNEL_WHITE].current_physical_state ? s_channels[LED_CHANNEL_WHITE].brightness : 0;

            xSemaphoreGive(s_led_mutex);

            // 실제 물리 LED 하드웨어에 색상 드라이브
            ws2812_driver_set_pixel(0, r, g, b, w);
            ws2812_driver_refresh();
        }
    }
}

esp_err_t led_control_init(int gpio_num)
{
    // 1. 드라이버 하위 계층 초기화
    esp_err_t err = ws2812_driver_init(gpio_num, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WS2812 driver: %s", esp_err_to_name(err));
        return err;
    }

    // 2. 뮤텍스 및 메시지 큐 생성
    s_led_mutex = xSemaphoreCreateMutex();
    if (s_led_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED Mutex");
        return ESP_ERR_NO_MEM;
    }

    s_led_cmd_queue = xQueueCreate(10, sizeof(led_cmd_msg_t));
    if (s_led_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create LED command queue");
        vSemaphoreDelete(s_led_mutex);
        s_led_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 3. LED 백그라운드 태스크 생성
    BaseType_t ret = xTaskCreate(
        led_control_task,
        "led_control_task",
        3072,
        NULL,
        5,
        NULL
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED Control Task");
        vQueueDelete(s_led_cmd_queue);
        s_led_cmd_queue = NULL;
        vSemaphoreDelete(s_led_mutex);
        s_led_mutex = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED Control (FreeRTOS & Blink Active) initialized successfully on GPIO %d", gpio_num);
    return ESP_OK;
}

esp_err_t led_control_send_cmd(led_channel_t channel, bool is_on, uint8_t brightness, uint32_t blink_delay_ms)
{
    if (s_led_cmd_queue == NULL) {
        ESP_LOGE(TAG, "LED Queue not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    led_cmd_msg_t msg = {
        .channel = channel,
        .is_on = is_on,
        .brightness = brightness,
        .blink_delay_ms = blink_delay_ms
    };

    if (xQueueSend(s_led_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "LED command queue is full");
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool led_control_get_channel_state(led_channel_t channel, uint8_t *out_brightness)
{
    if (channel < 0 || channel > LED_CHANNEL_WHITE || s_led_mutex == NULL) {
        return false;
    }

    bool is_on = false;

    if (xSemaphoreTake(s_led_mutex, portMAX_DELAY) == pdTRUE) {
        if (out_brightness != NULL) {
            *out_brightness = s_channels[channel].brightness;
        }
        is_on = s_channels[channel].is_on;
        xSemaphoreGive(s_led_mutex);
    }

    return is_on;
}
