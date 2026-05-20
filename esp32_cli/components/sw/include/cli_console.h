#ifndef CLI_CONSOLE_H
#define CLI_CONSOLE_H

#include "esp_err.h"

/**
 * @brief UART CLI REPL 콘솔 초기화 및 구동
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t cli_console_start(void);

#endif // CLI_CONSOLE_H
