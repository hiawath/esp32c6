#ifndef WIFI_APP_H
#define WIFI_APP_H

#include "esp_err.h"

/**
 * @brief WiFi STA 모드 초기화 및 접속 시작
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t wifi_app_init(void);

#endif // WIFI_APP_H
