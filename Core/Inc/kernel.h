#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

#define STACK_SIZE 4096 // 128 words or 4096 bytes
#define MAX_TASKS 128

typedef struct {
    _Alignas(8) uint8_t data[STACK_SIZE];           // member aligned
} __attribute__((aligned(8))) TaskStack; // type also aligned

typedef struct {
    uint32_t tid;
    char* task_name; 
    uint32_t* sp; 
    TaskStack taskStack;
} TCB;



void tiny_scheduler();
void context_switch();
