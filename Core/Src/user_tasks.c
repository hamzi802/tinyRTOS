

#include "stm32f4xx_hal.h"
#include "kernel.h"
#include "stdio.h"

// MEDIUM priority
void task1(void) {
    // printf("Running task 1");
    while(1){
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
        rtosTaskDelay(200);
    }
}


// low priority
void task2(void) {
    // printf("Running task 2\n");
    while(1){
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        rtosTaskDelay(10);
    }
}


// HIGHEST priority
void task3(void) {
    // printf("Running task3\n");
    while(1) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);
        rtosTaskDelay(150);
    }
}

