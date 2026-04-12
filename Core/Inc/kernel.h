#pragma once

#include <stdint.h>
#include <sys/cdefs.h>
#include <stdbool.h>

#define STACK_SIZE 2048 // 128 words or 4096 bytes
#define MAX_TASKS 8
#define MAX_PRIORITIES 3


typedef enum{
    PRIORITY_HIGH,
    PRIORITY_MEDIUM,
    PRIORITY_LOW,    
    PRIORITY_IDLE
}TaskPriority;

typedef enum{
    TASK_READY,
    TASK_BLOCKED,
    TASK_TIME_DELAY
} TaskState ;

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


extern TCB* prevTCB;
extern TCB* currTCB;

typedef struct TCBPoolObj {
    TCB tcb;
    struct TCBPoolObj* next;
} TCBPoolObj;


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

// Blocked Queue
void BlockedQueue_remove(TCB* tcb);
void BlockedQueue_insert(TCB* task);

// TIME DELAY QUEUE
void rtosTaskDelay(uint32_t delay_ticks);
bool delayTaskReady();
void activateDelayTask();

// Scheduler
void tiny_scheduler();
void start_scheduler();
void context_switch();
bool rtos_wake_task_from_isr(TCB* task);
void rtos_request_context_switch(void);
