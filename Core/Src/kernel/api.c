#include "kernel.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.h"
#include "assert.h"
#include "main.h"
#include "stdio.h"
#include "stm32f4xx_hal.h"



TCB idleTCB;

// Object pool for TCB
TCBPoolObj tcb_pool[MAX_TASKS] = {0}; 
TCBPoolObj* freelist = NULL;

// CALL THIS IN INIT CODE OF STM32 to run at startup right away
void InitializePool() {
    for (int i = 0; i < MAX_TASKS - 1; i++) {
        // setting id
        tcb_pool[i].tcb.tid = i;

        // setting next in each TCBPoolObj
        tcb_pool[i].next = &(tcb_pool[i+1]);
    }

    tcb_pool[MAX_TASKS-1].tcb.tid = (MAX_TASKS-1);

    // freelist = tcb_pool; // equivalently
    freelist = &(tcb_pool[0]);
    tcb_pool[MAX_TASKS-1].next = NULL;
}


TCB* borrowTCB(void) {

    if (freelist != NULL)  {
        // remove a obj from freelist
        TCBPoolObj* result = freelist;
        freelist = freelist->next;
        return &(result->tcb);
    }

    // if all objects are borrowed from the pool.
    return NULL;
}

void returnTCB(TCB* tcb) {
    unsigned int i = ((uintptr_t) tcb - (uintptr_t) tcb_pool) / sizeof(TCBPoolObj);

    assert(&(tcb_pool[i].tcb) == tcb);

    // adding the obj to the start of the free list 
    TCBPoolObj* objToAdd = &(tcb_pool[i]);
    objToAdd->next = freelist;
    freelist = objToAdd;
}


// -----------------------------------------
// READY QUEUES

// ready_queue contain number of queues according to priorities.
// Each element of ready_queue holds a queue which holds static array
// to use as circular queue to hold TCB pointers.

// This static array is defined by the ready_queuesDB 2D array.

TCB* ready_queuesDB[MAX_PRIORITIES][MAX_TASKS]; // 2D array of TCB*: each arr used by each ready_queue
queue ready_queues[MAX_PRIORITIES];

void initReadyQueue(TCB* readyqueuesDB[MAX_PRIORITIES][MAX_TASKS], queue* ready_queues) {
    for (int i = 0; i < MAX_PRIORITIES;i++) {
        init_queue(&ready_queues[i], readyqueuesDB[i], MAX_TASKS);
    }
}

TCB* ReadyQueue_popTCB() {
    TCB* res;

    for (int i = 0; i < MAX_PRIORITIES; i++) {
        if ((res = dequeue(&ready_queues[i])) != NULL) {
            return res;
        }
    }

    return &idleTCB;
}


TCB* ReadyQueue_getNextTCB() {
    TCB* res;

    for (int i = 0; i < MAX_PRIORITIES; i++) {
        if ((res = getHead(&ready_queues[i])) != NULL) {
            return res;
        }
    }

    return NULL;
}


void ReadyQueue_pushTCB(TCB* tcb) {
    if (tcb == &idleTCB) {
        return;
    }
    enqueue(&ready_queues[tcb->priority], tcb);
}


// BLOCKED QUEUE using Linked List
TCBNodeBLL BLL_tcb_pool[MAX_TASKS];
TCBNodeBLL BLL_head; // sentinal node
int BLL_length = 0;

// TODO: Handle some errors fool 
void BlockedQueue_remove(TCB* tcb) {
    BLL_delete(&BLL_head, tcb, &BLL_length);
}

// TODO: Handle some errors fool 
void BlockedQueue_insert(TCB* task) {
    BLL_insert(&BLL_head, task, &BLL_length);
}


// ------------------------------------------
// DELAY PRIORITY QUEUE
TCBNodeDPQ pq_DELAY_pool[MAX_TASKS]; // priority queue pool for blocked tasks 
TCBNodeDPQ pq_DELAY_active_head;  // sentinel for sorted active list
TCBNodeDPQ pq_DELAY_free_head;    // sentinel for free list
uint32_t pq_DELAY_length = 0;


void rtosTaskDelay(uint32_t delay_ticks) {
    currTCB->state = TASK_TIME_DELAY;
    // uint32_t psp_before;
    // __asm volatile("MRS %0, PSP" : "=r"(psp_before));
    // printf("PSP before insert: 0x%08lX\n", psp_before);
    
    __disable_irq();
    uint32_t wake_up = HAL_GetTick() + delay_ticks;
    pq_insert(&pq_DELAY_active_head, &pq_DELAY_free_head, &pq_DELAY_length,  currTCB, wake_up);
    __enable_irq();

    // uint32_t psp_after;
    // __asm volatile("MRS %0, PSP" : "=r"(psp_after));
    // printf("PSP after insert:  0x%08lX\n", psp_after);

    rtos_request_context_switch();
}

bool delayTaskReady() {
    TCBNodeDPQ* pqDelayHead = pq_getHead(&pq_DELAY_active_head);

    if ( pqDelayHead != NULL && pqDelayHead->delay_ticks <= HAL_GetTick()) {
        // printf("DELAY TASK READY\n");
        return true;
    }
    return false;
}

void activateDelayTask() {
    // printf("pq_length before=%lu\n", pq_DELAY_length);
    TCB* tcb= pq_dequeue(&pq_DELAY_active_head, &pq_DELAY_free_head, &pq_DELAY_length);     
    // printf("pq_length after=%lu\n", pq_DELAY_length);
    if (tcb == NULL) { // no task in delay queue. 
        // printf("NO TASK IN DELAY QUEUE\n");
        return;
    } 

    // printf("TASK become ready from being delay: %s\n", tcb->task_name);
    tcb->state = TASK_READY;
    // printf("restoring: %s sp=0x%08lX\n", tcb->task_name, (uint32_t)tcb->sp);
    ReadyQueue_pushTCB(tcb);
}


// --------------------------
// TASKS
// NOTE: THIS SHOULD NEVER TAKE THE ARGUMENT OF STATE BEING TIME_DELAY
TCB* createTask(char* task_name, TaskRoutine_t task_routine, TaskPriority priority, TaskState state) {

    // First we borrow a TCB to use from our TCB pool.
    TCB* tcb;
     
    if ((tcb = borrowTCB()) == NULL) {
        // do something in case we don't find a TCB block to allocate to our new task
        return NULL;
    }

    
    tcb->task_name = task_name;
    tcb->sp = NULL;
    tcb->priority = priority;
    tcb->state = state;


    // we do the following so that sp points to top of the stack. 

    // type conversion done because tcb.taskStack.data is uint8. Stack size is also in bytes.
    tcb->sp = (uint32_t*) (tcb->taskStack.data + STACK_SIZE);  



    // From our context switch code, we remember that when the scheduler will switch to this 
    // new task, it would first pop R4-R11 and then when the exception returns, it will 
    // pop the following register 8 registers from SP (PSP as we set it):   
    // xPSR → PC → LR → R12 → R3 → R2 → R1 → R0
    
    // Now, we want to give cortex M4 the illusion that this new task had already been 
    // once so thus we fake this frame stack for a task that never ran so that the CPU will 
    // return into it as if resuming.

    // For this, we need to do
    // 1. First set the sp such that 16 registers are already pushed into it.

    // task.sp is uint32_t* so compiler know, the address pointed to is 4 bytes word  
    tcb->sp = tcb->sp - 16;

    // TODO: IS THIS SAFE???
    *(tcb->sp + 13) = 0xFFFFFFFD; // setting LR to valid value instead of 0x00000000 which causes fault

    // set the PC to address of the routine of the task we want to run
    // In the stack, from top, the 15th register is PC so 

    *(tcb->sp + 14) = ((uint32_t) task_routine) | 1;    // tcb->sp[14] = (uint32_t) task_routine;

    // set the xPSR (the 16th register) to 0x01000000
    // 0x01000000 basically sets the Thumb Mode xPSR bit to 1 so that 
    // CPU uses the Thumb mode.
    *(tcb->sp + 15) = 0x01000000;

    // BY GPT
    // ---- Optional but GOOD ----
    tcb->sp[12] = 0xCCCCCCCC; // R12
    tcb->sp[11] = 0x33333333; // R3
    tcb->sp[10] = 0x22222222; // R2
    tcb->sp[9]  = 0x11111111; // R1
    tcb->sp[8]  = 0x00000000; // R0

    // ---- Software saved (R4-R11) ----
    tcb->sp[0] = 0;
    tcb->sp[1] = 0;
    tcb->sp[2] = 0;
    tcb->sp[3] = 0;
    tcb->sp[4] = 0;
    tcb->sp[5] = 0;
    tcb->sp[6] = 0;
    tcb->sp[7] = 0;


    // TODO: insert into the block queue if the state is blocked. 
    // Finally, insert this into our ready tasks queue based on priority.
    switch (state) {
        case TASK_READY:
            ReadyQueue_pushTCB(tcb);
        break;
        case TASK_BLOCKED:
            BLL_insert(&BLL_head, tcb, &BLL_length);
        break;    
    }

    // printf("Created task named: %s\n", task_name);

    return tcb;
}

void idleTaskRoutine() {
    // printf("Running idle task\n");
    while(1) {
        __WFI();  // no printf, no stack usage
    };
}

void initIdleTCB() {
    idleTCB.tid = MAX_TASKS;
    idleTCB.task_name = "idle";
    idleTCB.priority = PRIORITY_IDLE;
    idleTCB.state = TASK_READY;

    idleTCB.sp = (uint32_t*) (idleTCB.taskStack.data + STACK_SIZE);  
    idleTCB.sp = idleTCB.sp - 16;
    *(idleTCB.sp + 13) = 0xFFFFFFFD;  // ← ADD THIS LINE
    *(idleTCB.sp + 14) = ((uint32_t) idleTaskRoutine) | 1;
    *(idleTCB.sp + 15) = 0x01000000;
}

// -------------------------------------
// KERNEL

void kernel_init() {
    InitializePool();
    initReadyQueue(ready_queuesDB, ready_queues);
    init_pq(pq_DELAY_pool, MAX_TASKS, &pq_DELAY_free_head, &pq_DELAY_active_head);
    BLL_init(BLL_tcb_pool, MAX_TASKS, &BLL_head);

    initIdleTCB();

    // printf("Kernel init done\n");
    // printf("idleTCB.sp        = %p\n", idleTCB.sp);
    // printf("idleTaskRoutine   = %p\n", idleTaskRoutine);
    // printf("PC slot value     = 0x%08X\n", *(idleTCB.sp + 14));
}