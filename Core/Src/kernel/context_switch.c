#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "main.h"


TCB* prevTCB = NULL;
TCB* currTCB = NULL;

// uint32_T time_slice = 0;

volatile uint32_t debug_switches = 0;
volatile uint32_t debug_pendsv_fired = 0;
volatile char* debug_last_prev = NULL;
volatile char* debug_last_curr = NULL;


// Assumed: currTCB not NULL
// 1. check if time slice done
// 2. set prevTCB to currTCB
// 3. push currTCB to the ready queue
// 4. take a tcb from ready queue.
// 5. if prev and current same, no need for switch. This happens when a ready queue of some priority has one task only.
// 6. since, they are not same, push previous task to the ready_queue 
// 7. perform context switch from the prev task to the new task

void tiny_scheduler() {

    // Slice ocunter
    static uint32_t slice = 0;
    
    while (delayTaskReady()) {
        activateDelayTask();
        rtos_request_context_switch();
        return;
    }

    slice++;
    if (slice < 10) return;  // give task 10ms of CPU time
    slice = 0;

    // Here, we expect currTCB to have been set up by the start scdeduler function. 
    // TODO: There can be case like, where we don't do context switch if there are no more tasks.
    // FOR SUCH a condiiton, we need to implement a idle task. (inpirtaion: freeRTOS)

    // Time slice logic
    // time_slice++;

    // if a task delay is now ready, put it in the ready queue after popping it from the 
    // blocked due to time delay queue. 
    while (delayTaskReady()) {
        activateDelayTask(); // delay queue -> ready queue
    }

    // if (time_slice < 5) {
    //     return;
    // }

    // time_slice = 0; // reset time_slice

    rtos_request_context_switch();
}

// This is run by the main file or by the kernel_init();
// It simulates returning from an exception. 
void start_scheduler() {
    currTCB = ReadyQueue_popTCB();

    __asm volatile("SVC #0\n");
    // We do EXC_RETURN now. The CPU kind of abandons this fucntion after we return by simulating this as exception return.
    // __asm volatile(
    //     "MOV LR, #0xFFFFFFFD\n"  // Return to Thread mode using PSP
    //     "BX LR\n"
    // );
}



// __attribute__((naked))  void context_switch() {
//     __asm volatile(
//         // Save current context
//         "CPSID I\n" // disable intrrupts
//         "MRS R0, PSP\n"          // R0 = PSP
//         "STMDB R0!, {R4-R11}\n"    // store R4-R11 in PSP
//         "LDR R1, =prevTCB\n"     // R0 = address of prevTCB sp variable (prevTCB = prevTCB->sp)
//         "LDR R1, [R1]\n"        // R1 = prevTCB  (the TCB*)
//         "STR R0, [R1]\n"           // save PSP into current task's SP given by [prevTCB->sp variable]
        
//         // Switch PSP
//         "LDR R1, =currTCB\n"     // R0 = address of newTaskSP variable
//         "LDR R1, [R1]\n"         // value of newTaskSP variable 
//         "LDR R0, [R1]\n"        // R0 = currTCB  (the TCB*)
//         // "MSR PSP, R0\n"          // load the next Task SP into PSP: PSP = R0
        
//         // restore context
//         "LDMIA R0!, {R4-R11}\n"  // RePop the values into registers saved in the last context switch.
//         "MSR PSP, R0\n"            // After poping from the PSP, we have to update the PSP as well.

//         "CPSIE I\n" // enable intrrupts -- no need they are done automatically
//         // Return
//         "BX LR\n"
//     );
// }


bool rtos_wake_task_from_isr(TCB* task) {

    task->state = TASK_READY;

    BlockedQueue_remove(task);
    ReadyQueue_pushTCB(task);

    // < shows the task has higher priority
    if (task->priority < currTCB->priority) {
        return true;
    }

    return false;
}

void rtos_request_context_switch(void) {
    // time_slice = 0; // reset time_slice
    TCB* temp = prevTCB;
    prevTCB = currTCB;

    // We get a task from ready queue. if there's none, we get the IDLE task 
    currTCB = ReadyQueue_popTCB();

    // This occurs when a HIGH priority queue only have 1 task in ready state.
    // we keep running this task until this task gets removed from ready state.
    if (currTCB == prevTCB) { 
        prevTCB = temp;
        // SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;  // CLAUDE
        return;
    }

    // CLAUDE
    // if (prevTCB == NULL) {
    //     prevTCB = currTCB;
    //     return;
    // }

    // check which queue should the prev TCB go 
    if (prevTCB != NULL) { 
        switch (prevTCB->state) {
            case TASK_READY: 
                ReadyQueue_pushTCB(prevTCB);
                break;
            // TODO: case TASK_BLOCKED, 
            case TASK_TIME_DELAY:
                // IGNORE THE CURR_TCB IN THIS CASE AS THIS STATE IS ONLY SET UP BY THE 
                // taskDelay() which puts the currTCB in the delay queue.
            break;
            case TASK_BLOCKED:
                BlockedQueue_insert(prevTCB);
            break;
        }
    } 
    debug_last_prev = prevTCB != NULL ? prevTCB->task_name : "idle";
    debug_last_curr = currTCB != NULL ? currTCB->task_name : "idle";
    debug_switches++;

    // Trigger Context Switch: Pend the PendSV handler
    if (!(SCB->ICSR & SCB_ICSR_PENDSVSET_Msk)) {
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
}