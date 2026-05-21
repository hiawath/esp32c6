#include "telnet_server.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "cli_console.h"

static const char *TAG = "telnet_server";

static void telnet_session_task(void *pvParameters)
{
    int client_sock = (int)pvParameters;
    ESP_LOGI(TAG, "New Telnet session started for socket %d", client_sock);

    // stdout, stderr 스트림을 소켓 스트림으로 변경하기 위한 백업
    FILE *orig_stdout = stdout;
    FILE *orig_stderr = stderr;

    // 소켓 디스크립터를 FILE 스트림으로 변환
    FILE *sock_file = fdopen(client_sock, "w");
    if (sock_file == NULL) {
        ESP_LOGE(TAG, "Failed to fdopen socket %d for writing", client_sock);
        close(client_sock);
        vTaskDelete(NULL);
        return;
    }

    // 전역 표준 출력 리디렉션 백업 적용
    stdout = sock_file;
    stderr = sock_file;

    // 텔넷 글로벌 프린트 스트림 지정
    g_telnet_stdout = sock_file;

    // Telnet 프로토콜 협상 옵션 전송 (RFC 854)
    // 255(IAC) 251(WILL) 1(ECHO): 보드가 직접 에코백을 하겠다고 선언 (클라이언트 로컬 에코 방지)
    // 255(IAC) 251(WILL) 3(SGA): Suppress Go Ahead 허용 선언
    // 255(IAC) 253(DO) 3(SGA): 클라이언트에게 SGA 모드 활성화 요청 (문자 입력 시 즉시 송신 유도)
    const uint8_t telnet_options[] = {
        255, 251, 1,   // IAC WILL ECHO
        255, 251, 3,   // IAC WILL SGA
        255, 253, 3    // IAC DO SGA
    };
    send(client_sock, telnet_options, sizeof(telnet_options), 0);

    // Telnet 접속 환영 안내 (Premium Theme)
    fprintf(sock_file, "\r\n\033[1;36m============================================\033[0m\r\n");
    fprintf(sock_file, "\033[1;32m    ESP32-C6 Remote Telnet CLI Console\033[0m\r\n");
    fprintf(sock_file, "\033[1;36m============================================\033[0m\r\n");
    fprintf(sock_file, "Type '\033[1;33mhelp\033[0m' to see list of commands.\r\n");
    fprintf(sock_file, "Type '\033[1;31mexit\033[0m' to end this connection.\r\n\r\n");
    fprintf(sock_file, "esp32c6> ");
    fflush(sock_file);

    char line_buf[128];
    int line_len = 0;
    char recv_buf[64];

    // 히스토리 제어 변수 선언
    #define HISTORY_MAX 10
    char history[HISTORY_MAX][128];
    int history_count = 0;
    int history_index = 0;

    // ESC 시퀀스 및 텍스트 시퀀스 파싱 상태 머신
    typedef enum {
        STATE_NORMAL,
        STATE_ESC,
        STATE_TXT_ESC,
        STATE_BRACKET
    } input_state_t;
    input_state_t input_state = STATE_NORMAL;
    bool is_text_sequence = false;

    while (1) {
        // 소켓으로부터 로우 데이터를 직접 안전하게 수신
        int len = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        if (len <= 0) {
            ESP_LOGI(TAG, "Client disconnected or EOF on socket %d", client_sock);
            break;
        }

        for (int i = 0; i < len; i++) {
            uint8_t c = recv_buf[i];

            // Telnet 협상 명령어 바이트(IAC) 무시 (\xff)
            if (c == 255) {
                i += 2; // 이어지는 2바이트 명령 옵션 무시
                continue;
            }

            // ESC 시퀀스 및 텍스트 시퀀스 파싱 상태 처리
            if (input_state == STATE_ESC) {
                if (c == '[') {
                    input_state = STATE_BRACKET;
                } else {
                    input_state = STATE_NORMAL;
                }
                continue;
            } else if (input_state == STATE_TXT_ESC) {
                if (c == '[') {
                    input_state = STATE_BRACKET;
                } else {
                    input_state = STATE_NORMAL;
                    if (line_len < sizeof(line_buf) - 2) {
                        line_buf[line_len++] = '^';
                        line_buf[line_len++] = c;
                        fprintf(sock_file, "^%c", c);
                        fflush(sock_file);
                    }
                }
                continue;
            } else if (input_state == STATE_BRACKET) {
                if (c == '[') {
                    // 중첩된 브래킷 문자 처리 우회
                    continue;
                }
                input_state = STATE_NORMAL;
                if (c == 'A' || c == 'B' || c == 'C' || c == 'D') {
                    if (is_text_sequence) {
                        // 텍스트 형태로 화면에 에코되어 버린 찌꺼기 "^[[" 혹은 "^[" 텍스트를 청소 (3글자 소거)
                        fprintf(sock_file, "\b\b\b   \b\b\b");
                        fflush(sock_file);
                        is_text_sequence = false;
                    }

                    if (c == 'A') { // Up Arrow
                        if (history_count > 0 && history_index > 0) {
                            history_index--;
                            // 현재 입력창 지우기
                            for (int k = 0; k < line_len; k++) {
                                fprintf(sock_file, "\b \b");
                            }
                            // 히스토리 로드
                            int idx = history_index % HISTORY_MAX;
                            strcpy(line_buf, history[idx]);
                            line_len = strlen(line_buf);
                            // 새로 출력
                            fprintf(sock_file, "%s", line_buf);
                            fflush(sock_file);
                        }
                    } else if (c == 'B') { // Down Arrow
                        if (history_index < history_count) {
                            history_index++;
                            // 현재 입력창 지우기
                            for (int k = 0; k < line_len; k++) {
                                fprintf(sock_file, "\b \b");
                            }
                            if (history_index == history_count) {
                                // 빈 라인 복원
                                line_buf[0] = '\0';
                                line_len = 0;
                            } else {
                                // 히스토리 로드
                                int idx = history_index % HISTORY_MAX;
                                strcpy(line_buf, history[idx]);
                                line_len = strlen(line_buf);
                                // 새로 출력
                                fprintf(sock_file, "%s", line_buf);
                            }
                            fflush(sock_file);
                        }
                    }
                    // C, D (좌우 화살표)는 조용히 흔적만 청소하고 무시함
                }
                continue;
            }

            // ESC 키 감지 시 시퀀스 대기로 상태 전이
            if (c == 27) {
                input_state = STATE_ESC;
                is_text_sequence = false;
                continue;
            }

            // 아스키 문자 '^' 감지 시 텍스트 시퀀스 대기로 상태 전이
            if (c == '^') {
                input_state = STATE_TXT_ESC;
                is_text_sequence = true;
                continue;
            }

            if (c == '\r' || c == '\n') {
                if (line_len > 0) {
                    line_buf[line_len] = '\0';
                    fprintf(sock_file, "\r\n");
                    fflush(sock_file);

                    // exit 명령어 처리
                    if (strcmp(line_buf, "exit") == 0) {
                        fprintf(sock_file, "Goodbye!\r\n");
                        fflush(sock_file);
                        goto exit_session;
                    }

                    // 히스토리에 명령어 기록 (중복 입력 방지)
                    bool should_add = true;
                    if (history_count > 0) {
                        int prev_idx = (history_count - 1) % HISTORY_MAX;
                        if (strcmp(history[prev_idx], line_buf) == 0) {
                            should_add = false;
                        }
                    }
                    if (should_add) {
                        int next_idx = history_count % HISTORY_MAX;
                        strncpy(history[next_idx], line_buf, sizeof(history[next_idx]) - 1);
                        history[next_idx][sizeof(history[next_idx]) - 1] = '\0';
                        history_count++;
                    }
                    history_index = history_count; // 신규 세션 상태로 인덱스 대기

                    // help 및 ? 명령어 커스텀 처리 (개행 및 레이아웃 최적화)
                    if (strcmp(line_buf, "help") == 0 || strcmp(line_buf, "?") == 0) {
                        fprintf(sock_file, "\r\n\033[1;32mSupported Commands:\033[0m\r\n");
                        fprintf(sock_file, "  \033[1;33mhelp\033[0m, \033[1;33m?\033[0m             Display this command list\r\n");
                        fprintf(sock_file, "  \033[1;33mled_on\033[0m            Turn on LED channel\r\n");
                        fprintf(sock_file, "                     Usage: led_on <r|g|b|w|all> [brightness(0-255)] [blink_delay_ms]\r\n");
                        fprintf(sock_file, "  \033[1;33mled_off\033[0m           Turn off LED channel\r\n");
                        fprintf(sock_file, "                     Usage: led_off <r|g|b|w|all>\r\n");
                        fprintf(sock_file, "  \033[1;33mled_status\033[0m        Show current status of all LED channels\r\n");
                        fprintf(sock_file, "  \033[1;33mexit\033[0m              Close this Telnet connection\r\n\r\n");
                        fflush(sock_file);

                        line_len = 0;
                        fprintf(sock_file, "esp32c6> ");
                        fflush(sock_file);
                        continue;
                    }

                    // 일반 CLI 명령어 실행 (do_led_on_cmd, do_led_status_cmd 등)
                    // 이 커맨드 핸들러들의 printf는 cli_printf에 의해 소켓 스트림으로 분기됩니다.
                    int ret_code = 0;
                    esp_err_t err = esp_console_run(line_buf, &ret_code);
                    
                    if (err == ESP_ERR_NOT_FOUND) {
                        fprintf(sock_file, "Error: Command not found. Type 'help' for support.\r\n");
                    } else if (err != ESP_OK) {
                        fprintf(sock_file, "Error: Execution failed: %s (code %d)\r\n", esp_err_to_name(err), ret_code);
                    }
                    fflush(sock_file);

                    line_len = 0;
                    fprintf(sock_file, "\r\nesp32c6> ");
                    fflush(sock_file);
                } else {
                    // 빈 줄 엔터 입력 시 중복 줄바꿈 방지
                    if (c == '\r') {
                        fprintf(sock_file, "\r\nesp32c6> ");
                        fflush(sock_file);
                    }
                }
            } else if (c == '\b' || c == 127) {
                // 백스페이스 및 DEL 키 에코백 처리
                if (line_len > 0) {
                    line_len--;
                    fprintf(sock_file, "\b \b");
                    fflush(sock_file);
                }
            } else if (c >= 32 && c <= 126 && line_len < sizeof(line_buf) - 1) {
                // 일반 글자 입력 및 즉각적인 에코백
                line_buf[line_len++] = c;
                fputc(c, sock_file);
                fflush(sock_file);
            }
        }
    }

exit_session:
    // 글로벌 텔넷 스트림 종료 해제
    g_telnet_stdout = NULL;

    // 리디렉션 원상복구
    stdout = orig_stdout;
    stderr = orig_stderr;

    fclose(sock_file);
    close(client_sock);
    ESP_LOGI(TAG, "Telnet session closed for socket %d", client_sock);
    vTaskDelete(NULL);
}

static void telnet_server_task(void *pvParameters)
{
    int listen_port = CONFIG_ESP_TELNET_PORT;
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(listen_port);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // 포트 재사용 옵션 적용
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created, binding to port %d", listen_port);
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    err = listen(listen_sock, 2);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Telnet Server listening on port %d...", listen_port);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        ESP_LOGI(TAG, "Socket accepted connection from %s", inet_ntoa(source_addr.sin_addr));

        // 접속한 세션을 독립적으로 핸들링할 FreeRTOS 태스크 동적 실행
        BaseType_t ret = xTaskCreate(
            telnet_session_task,
            "telnet_session",
            4096,
            (void *)client_sock,
            5,
            NULL
        );

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create telnet session task");
            close(client_sock);
        }
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

esp_err_t telnet_server_start(void)
{
    BaseType_t ret = xTaskCreate(
        telnet_server_task,
        "telnet_server_task",
        4096,
        NULL,
        5,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to start Telnet Server task");
        return ESP_FAIL;
    }

    return ESP_OK;
}
