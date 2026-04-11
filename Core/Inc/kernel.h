#pragma once

#include "kernel.h"
#include <stdint.h>
#include <sys/cdefs.h>

#define STACK_SIZE 4096 // 128 words or 4096 bytes
#define MAX_TASKS 16
#define MAX_PRIORITIES 3


extern TCB* prevTCB;
extern TCB* currTCB;

enum TaskPriority{
    PRIORITY_HIGH,
    PRIORITY_MEDIUM,
    PRIORITY_LOW,    
    PRIORITY_IDLE
}

enum TaskState {
    TASK_READY,
    TASK_BLOCKED,
    TASK_TIME_DELAY
}

typedef struct {
    _Alignas(8) uint8_t data[STACK_SIZE];           // member aligned
} __attribute__((aligned(8))) TaskStack; // type also aligned

typedef struct {
    uint32_t* sp;  // THIS MUST BE FIRST FIELD OF STRUCT: We Use TCB to access this is asm without offsets.
    uint32_t tid;
    char* task_name; 
    TaskPriority priority;
    TaskState state;
    TaskStack taskStack;
} TCB;

typedef struct TCBPoolObj TCBPoolObj;
struct TCBPoolObj{
    TCB tcb;
    TCBPoolObj* next;
};


// Kernel
void kernel_init();

// Tasks
typedef void (*TaskRoutine_t)(void);

TCB* createTask(char* task_name, TaskRoutine_t task_routine, TaskPriority priority, TaskState state);
void idleTaskRoutine();
void initIdleTCB();

// Ready Queues
TCB* ReadyQueue_popTCB();
void ReadyQueue_pushTCB(TCB* tcb);

// TIME DELAY QUEUE
bool delayTaskReady();
void activateDelayTask();

// Scheduler
void tiny_scheduler();
void start_scheduler();
void context_switch();
void rtos_request_context_switch(void);
