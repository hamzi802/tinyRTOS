#include "kernel.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.h"
#include "stm32f4xx_hal.h"
#include "assert.h"



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
    enqueue(&ready_queues[tcb->priority], tcb);
}


// ------------------------------------------
// DELAY PRIORITY QUEUE
TCBNodeDPQ pq_DELAY_pool[MAX_TASKS]; // priority queue pool for blocked tasks 
TCBNodeDPQ pq_DELAY_active_head;  // sentinel for sorted active list
TCBNodeDPQ pq_DELAY_free_head;    // sentinel for free list
int pq_DELAY_length = 0;


void taskDelay(uint32_t delay_ticks) {
    currTCB->state = TASK_BLOCKED;
    
    pq_insert(&pq_DELAY_active_head, &pq_DELAY_free_head, pq_DELAY_length,  currTCB, delay_ticks+HAL_GetTick());
    rtos_request_context_switch();
}

bool delayTaskReady() {
    TCBNodeDPQ* pqDelayHead = pq_getHead(&pq_DELAY_active_head);
    if ( pqDelayHead != NULL && pqDelayHead->delay_ticks == HAL_GetTick()) {
        return true;
    }
}

void activateDelayTask() {
    TCBNodeDPQ* tcbnode = pq_dequeue(&pq_DELAY_active_head, &pq_DELAY_free_head, pq_DELAY_length);     
    if (tcbnode == NULL) return; // no task in delay queue. 

    tcbnode->tcb->state = TASK_READY;
    ReadyQueue_pushTCB(tcbnode->tcb);
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

    // set the PC to address of the routine of the task we want to run
    // In the stack, from top, the 15th register is PC so 

    *(tcb->sp + 14) = (uint32_t) task_routine;    // tcb->sp[14] = (uint32_t) task_routine;

    // set the xPSR (the 16th register) to 0x01000000
    // 0x01000000 basically sets the Thumb Mode xPSR bit to 1 so that 
    // CPU uses the Thumb mode.
    *(tcb->sp + 15) = 0x01000000;


    // TODO: insert into the block queue if the state is blocked. 
    // Finally, insert this into our ready tasks queue based on priority.
    enqueue(&ready_queues[priority], tcb);

    return tcb;
}

void idleTaskRoutine() {
    while(1);
}

void initIdleTCB() {
    idleTCB.tid = MAX_TASKS;
    idleTCB.task_name = "idle";
    idleTCB.priority = PRIORITY_IDLE;
    idleTCB.state = TASK_READY;

    idleTCB.sp = (uint32_t*) (idleTCB.taskStack.data + STACK_SIZE);  
    idleTCB.sp = idleTCB.sp - 16;
    *(idleTCB.sp + 14) = (uint32_t) idleTaskRoutine;
    *(idleTCB.sp + 15) = 0x01000000;
}

// -------------------------------------
// KERNEL

void kernel_init() {
    InitializePool();
    initReadyQueue(ready_queuesDB, ready_queues);
    initIdleTCB();

    init_pq(pq_DELAY_pool, pq_DELAY_length, &pq_DELAY_free_head, &pq_DELAY_active_head);
}