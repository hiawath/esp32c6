#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// LED 채널 정의
typedef enum {
    LED_CHANNEL_RED = 0,
    LED_CHANNEL_GREEN,
    LED_CHANNEL_BLUE,
    LED_CHANNEL_WHITE,
    LED_CHANNEL_ALL,
    LED_CHANNEL_MAX
} led_channel_t;

// FreeRTOS 메시지 큐용 명령 구조체
typedef struct {
    led_channel_t channel;
    bool is_on;
    uint8_t brightness;
    uint32_t blink_delay_ms; // 깜빡임 간격 (ms). 0 이면 깜빡이지 않음
} led_cmd_msg_t;

/**
 * @brief LED HW 및 드라이버 초기화, FreeRTOS 태스크 및 큐 생성
 * 
 * @param gpio_num LED 제어에 사용할 GPIO 핀 번호
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t led_control_init(int gpio_num);

/**
 * @brief FreeRTOS 메시지 큐를 통해 LED 제어 명령 전송 (비동기 방식)
 * 
 * @param channel 제어할 채널 (RED, GREEN, BLUE, WHITE, ALL)
 * @param is_on ON/OFF 여부
 * @param brightness 밝기 값 (0~255)
 * @param blink_delay_ms 깜빡임 주기 (ms). 0 이면 항상 켜짐/꺼짐
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드 (큐 가득 참 등)
 */
esp_err_t led_control_send_cmd(led_channel_t channel, bool is_on, uint8_t brightness, uint32_t blink_delay_ms);

/**
 * @brief 특정 채널의 현재 설정 상태 획득 (Mutex 동기화 적용됨)
 * 
 * @param channel 상태를 조회할 채널
 * @param out_brightness 설정된 밝기 값을 받을 포인터 (NULL 허용)
 * @return true 채널이 ON 상태
 * @return false 채널이 OFF 상태
 */
bool led_control_get_channel_state(led_channel_t channel, uint8_t *out_brightness);

#endif // LED_CONTROL_H
