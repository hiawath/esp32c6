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

/**
 * @brief LED HW 및 드라이버 초기화
 * 
 * @param gpio_num LED 제어에 사용할 GPIO 핀 번호
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t led_control_init(int gpio_num);

/**
 * @brief 특정 채널의 ON/OFF 및 밝기 설정
 * 
 * @param channel 제어할 채널 (RED, GREEN, BLUE, WHITE, ALL)
 * @param is_on ON/OFF 여부
 * @param brightness 밝기 값 (0~255)
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t led_control_set_channel(led_channel_t channel, bool is_on, uint8_t brightness);

/**
 * @brief 변경 사항을 실제 하드웨어 LED에 적용
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t led_control_apply(void);

/**
 * @brief 특정 채널의 현재 설정 상태 획득
 * 
 * @param channel 상태를 조회할 채널
 * @param out_brightness 설정된 밝기 값을 받을 포인터 (NULL 허용)
 * @return true 채널이 ON 상태
 * @return false 채널이 OFF 상태
 */
bool led_control_get_channel_state(led_channel_t channel, uint8_t *out_brightness);

#endif // LED_CONTROL_H
