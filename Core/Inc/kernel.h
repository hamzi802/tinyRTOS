#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

#define STACK_SIZE 4096 // 128 words or 4096 bytes
#define MAX_TASKS 16
#define MAX_PRIORITIES 3

enum TaskPriority{
    HIGH,
    MEDIUM,
    LOW    
}

typedef struct {
    _Alignas(8) uint8_t data[STACK_SIZE];           // member aligned
} __attribute__((aligned(8))) TaskStack; // type also aligned

// TIP from freeRTOS source code:
typedef struct {
    uint32_t* sp;  // THIS MUST BE FIRST FIELD OF STRUCT: We Use TCB to access this is asm without offsets.
    uint32_t tid;
    char* task_name; 
    TaskPriority priority;
    TaskStack taskStack;
} TCB;


typedef struct {
    TCB tcb;
    TCBPoolObj* next;
} TCBPoolObj;


// Kernel
void kernel_init();

// Tasks
typedef void (*TaskRoutine_t)(void);

void createTask(char* task_name, TaskRoutine_t task_routine, TaskPriority priority);

// Ready Queues
TCB* ReadyQueue_popTCB();
void ReadyQueue_pushTCB(TCB* tcb);

// Scheduler
void tiny_scheduler();
void start_scheduler();
void context_switch();
