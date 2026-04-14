

#include "stm32f4xx_hal.h"
#include "kernel.h"
#include "stdio.h"

// MEDIUM priority
void task1(void) {
    while(1){
        printf("task1 running. switches=%lu pendsv=%lu last=%s->%s\n",
               debug_switches, debug_pendsv_fired,
               debug_last_prev, debug_last_curr);
        // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
        rtosTaskDelay(200);
    }
}


// low priority
void task2(void) {
    while(1){
        printf("Running task2\n");
        // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        rtosTaskDelay(10);
    }
}


// HIGHEST priority
void task3(void) {
    while(1) {
        printf("Running task3\n");
        // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);
        rtosTaskDelay(150);
    }
}

