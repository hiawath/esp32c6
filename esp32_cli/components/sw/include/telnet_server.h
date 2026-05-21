#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include "esp_err.h"

/**
 * @brief TCP Telnet CLI 서버 시작 (FreeRTOS 태스크 생성)
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t telnet_server_start(void);

#endif // TELNET_SERVER_H
