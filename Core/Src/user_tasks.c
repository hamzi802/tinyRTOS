

#include "stm32f4xx_hal.h"
#include "kernel.h"
#include "user_tasks.h"
#include "stdio.h"
#include <string.h>
#include <stdlib.h>

volatile uint8_t uart_cmd_ready = 0;
char uart_cmd_buffer[64] = {0};
volatile uint8_t board_led_on = 1;
volatile uint32_t task1_delay_ms = 300;
volatile uint32_t task2_delay_ms = 600;
volatile uint32_t task3_delay_ms = 100;

static void report_task(const char *name)
{
    printf("TASK,%s,%lu,HEARTBEAT\r\n", name, HAL_GetTick());
}

static void trim_cmd(char *text)
{
    int start = 0;
    int end = (int)strlen(text);

    while (start < end && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n')) {
        start++;
    }

    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n')) {
        end--;
    }

    if (start > 0) {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';
}

static volatile uint32_t *task_delay_for_name(const char *name)
{
    if (strcmp(name, "task1") == 0) {
        return &task1_delay_ms;
    }
    if (strcmp(name, "task2") == 0) {
        return &task2_delay_ms;
    }
    if (strcmp(name, "task3") == 0) {
        return &task3_delay_ms;
    }
    return NULL;
}

void set_board_led(uint8_t on)
{
    board_led_on = on;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void handle_uart_command(const char *cmd)
{
    char line[64];
    snprintf(line, sizeof(line), "%s", cmd);
    trim_cmd(line);

    if (line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0) {
        printf("SYS,help,OK,commands: help | status | led on | led off | toggle | tasklist | delay task1|task2|task3 ms\r\n");
    }
    else if (strcmp(line, "status") == 0) {
        printf("SYS,status,OK,uptime_ms=%lu,led=%s\r\n", HAL_GetTick(), board_led_on ? "on" : "off");
    }
    else if (strcmp(line, "tasklist") == 0) {
         printf("SYS,tasklist,OK,task1=%lu|task2=%lu|task3=%lu\r\n",
             task1_delay_ms, task2_delay_ms, task3_delay_ms);
    }
    else if (strcmp(line, "toggle") == 0) {
        set_board_led(!board_led_on);
        printf("SYS,toggle,OK,led=%s\r\n", board_led_on ? "on" : "off");
    }
    else if (strncmp(line, "led ", 4) == 0) {
        char *value = line + 4;
        trim_cmd(value);
        if (strcmp(value, "on") == 0) {
            set_board_led(1);
            printf("SYS,led,OK,led=on\r\n");
        }
        else if (strcmp(value, "off") == 0) {
            set_board_led(0);
            printf("SYS,led,OK,led=off\r\n");
        }
        else {
            printf("SYS,led,ERR,usage: led on|off\r\n");
        }
    }
    else if (strncmp(line, "delay ", 6) == 0) {
        char task_name[16] = {0};
        unsigned long delay_ms = 0;
        volatile uint32_t *delay = NULL;

        if (sscanf(line + 6, "%15s %lu", task_name, &delay_ms) != 2) {
            printf("SYS,delay,ERR,usage: delay task1|task2|task3 milliseconds\r\n");
        }
        else if (delay_ms == 0 || delay_ms > 60000) {
            printf("SYS,delay,ERR,milliseconds must be between 1 and 60000\r\n");
        }
        else if ((delay = task_delay_for_name(task_name)) == NULL) {
            printf("SYS,delay,ERR,unknown task: %s\r\n", task_name);
        }
        else {
            *delay = (uint32_t)delay_ms;
            printf("SYS,delay,OK,%s=%lu\r\n", task_name, delay_ms);
        }
    }
    else {
        printf("SYS,cmd,ERR,unknown command: %s\r\n", line);
    }
}

void uart_command_task(void)
{
    printf("SYS,banner,OK,tinyRTOS shell ready. Type 'help'\r\n");

    while (1) {
        if (uart_cmd_ready) {
            uart_cmd_ready = 0;
            handle_uart_command(uart_cmd_buffer);
            memset(uart_cmd_buffer, 0, sizeof(uart_cmd_buffer));
        }
        rtosTaskDelay(20);
    }
}

// MEDIUM priority
void task1(void) {
    while(1){
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        report_task("task1");
        rtosTaskDelay(task1_delay_ms);
    }
}


// low priority
void task2(void) {
    while(1){
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        report_task("task2");
        rtosTaskDelay(task2_delay_ms);
    }
}


// HIGHEST priority
void task3(void) {
    while(1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        report_task("task3");
        rtosTaskDelay(task3_delay_ms);
    }
}

