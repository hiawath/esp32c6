#ifndef CLI_CONSOLE_H
#define CLI_CONSOLE_H

#include "esp_err.h"
#include <stdio.h>
#include <stdarg.h>

extern FILE *g_telnet_stdout;

/**
 * @brief UART CLI REPL 콘솔 초기화 및 구동
 * 
 * @return esp_err_t ESP_OK 성공, 그 외 에러 코드
 */
esp_err_t cli_console_start(void);

/**
 * @brief 텔넷 세션 출력을 가로채기 위한 래퍼 프린트 함수
 */
int cli_printf(const char *format, ...);

#endif // CLI_CONSOLE_H
