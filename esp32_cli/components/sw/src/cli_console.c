#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cli_console.h"
#include "esp_console.h"
#include "esp_log.h"
#include "led_control.h"

static const char *TAG = "cli_console";

// r, g, b, w, all 문자열을 led_channel_t로 매핑하는 헬퍼 함수
static led_channel_t parse_channel(const char *str)
{
    if (strcasecmp(str, "r") == 0 || strcasecmp(str, "red") == 0) {
        return LED_CHANNEL_RED;
    } else if (strcasecmp(str, "g") == 0 || strcasecmp(str, "green") == 0) {
        return LED_CHANNEL_GREEN;
    } else if (strcasecmp(str, "b") == 0 || strcasecmp(str, "blue") == 0) {
        return LED_CHANNEL_BLUE;
    } else if (strcasecmp(str, "w") == 0 || strcasecmp(str, "white") == 0) {
        return LED_CHANNEL_WHITE;
    } else if (strcasecmp(str, "all") == 0) {
        return LED_CHANNEL_ALL;
    }
    return LED_CHANNEL_MAX;
}

static const char* channel_name(led_channel_t channel)
{
    switch (channel) {
        case LED_CHANNEL_RED: return "Red";
        case LED_CHANNEL_GREEN: return "Green";
        case LED_CHANNEL_BLUE: return "Blue";
        case LED_CHANNEL_WHITE: return "White";
        case LED_CHANNEL_ALL: return "All";
        default: return "Unknown";
    }
}

// 'led_on' 명령어 핸들러
static int do_led_on_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: led_on <r|g|b|w|all> [brightness(0-255)]\n");
        return 1;
    }

    led_channel_t channel = parse_channel(argv[1]);
    if (channel == LED_CHANNEL_MAX) {
        printf("Error: Invalid channel '%s'. Use r, g, b, w, or all.\n", argv[1]);
        return 1;
    }

    int brightness = 128; // default brightness
    if (argc >= 3) {
        int val = atoi(argv[2]);
        if (val < 0 || val > 255) {
            printf("Error: Brightness must be between 0 and 255.\n");
            return 1;
        }
        brightness = val;
    }

    led_control_set_channel(channel, true, brightness);
    led_control_apply();

    printf("LED %s channel ON (brightness: %d)\n", channel_name(channel), brightness);
    return 0;
}

// 'led_off' 명령어 핸들러
static int do_led_off_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: led_off <r|g|b|w|all>\n");
        return 1;
    }

    led_channel_t channel = parse_channel(argv[1]);
    if (channel == LED_CHANNEL_MAX) {
        printf("Error: Invalid channel '%s'. Use r, g, b, w, or all.\n", argv[1]);
        return 1;
    }

    led_control_set_channel(channel, false, 0);
    led_control_apply();

    printf("LED %s channel OFF\n", channel_name(channel));
    return 0;
}

// 'led_status' 명령어 핸들러
static int do_led_status_cmd(int argc, char **argv)
{
    printf("--- LED Status ---\n");
    for (int i = 0; i <= LED_CHANNEL_WHITE; i++) {
        uint8_t brightness = 0;
        bool is_on = led_control_get_channel_state(i, &brightness);
        printf("%-5s Channel: %s (Brightness: %d)\n", 
               channel_name(i), is_on ? "ON" : "OFF", brightness);
    }
    return 0;
}

esp_err_t cli_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp32-c6> ";
    repl_config.max_cmdline_length = 256;

    // UART REPL 설정
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    // UART0은 DevKit에서 기본 CLI 통신에 쓰임 (USB-to-UART bridge)
    uart_config.channel = 0; 
    uart_config.baud_rate = 115200;

    // REPL 환경 생성
    esp_err_t err = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create UART REPL: %s", esp_err_to_name(err));
        return err;
    }

    // 기본 명령어 등록 (help, version 등)
    esp_console_register_help_command();

    // 커스텀 CLI 명령어 등록
    const esp_console_cmd_t led_on_cmd = {
        .command = "led_on",
        .help = "Turn on LED channel (r, g, b, w, all)",
        .hint = "<r|g|b|w|all> [brightness(0-255)]",
        .func = &do_led_on_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&led_on_cmd));

    const esp_console_cmd_t led_off_cmd = {
        .command = "led_off",
        .help = "Turn off LED channel (r, g, b, w, all)",
        .hint = "<r|g|b|w|all>",
        .func = &do_led_off_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&led_off_cmd));

    const esp_console_cmd_t led_status_cmd = {
        .command = "led_status",
        .help = "Show current LED channel status",
        .func = &do_led_status_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&led_status_cmd));

    // REPL 구동 시작
    err = esp_console_start_repl(repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start REPL: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "UART CLI REPL Console started");
    return ESP_OK;
}
