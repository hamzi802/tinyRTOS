#pragma once


void task1(void);
void task2(void);
void task3(void);
void uart_command_task(void);

extern volatile uint8_t uart_cmd_ready;
extern char uart_cmd_buffer[64];
extern volatile uint8_t board_led_on;
extern volatile uint32_t task1_delay_ms;
extern volatile uint32_t task2_delay_ms;
extern volatile uint32_t task3_delay_ms;

void handle_uart_command(const char *cmd);
void set_board_led(uint8_t on);