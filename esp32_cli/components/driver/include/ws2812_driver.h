#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief WS2812 LED 드라이버 초기화
 * 
 * @param gpio_num LED 제어에 사용할 GPIO 핀 번호
 * @param led_count 제어할 LED 개수
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t ws2812_driver_init(int gpio_num, int led_count);

/**
 * @brief LED 픽셀 색상 설정 (RGBW)
 * 
 * @param index LED 인덱스 (0부터 시작)
 * @param red 빨간색 밝기 (0~255)
 * @param green 초록색 밝기 (0~255)
 * @param blue 파란색 밝기 (0~255)
 * @param white 흰색 밝기 (0~255)
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t ws2812_driver_set_pixel(int index, uint8_t red, uint8_t green, uint8_t blue, uint8_t white);

/**
 * @brief 변경된 색상 데이터를 하드웨어 LED에 적용
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t ws2812_driver_refresh(void);

/**
 * @brief 모든 LED를 끔 (0, 0, 0, 0)
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t ws2812_driver_clear(void);

#endif // WS2812_DRIVER_H
