#include "kernel.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal_cortex.h"
#include <stdint.h>
#include <stdbool.h>


TCB* prevTCB = NULL;
TCB* currTCB = NULL;

// uint32_T time_slice = 0;




// Assumed: currTCB not NULL
// 1. check if time slice done
// 2. set prevTCB to currTCB
// 3. push currTCB to the ready queue
// 4. take a tcb from ready queue.
// 5. if prev and current same, no need for switch. This happens when a ready queue of some priority has one task only.
// 6. since, they are not same, push previous task to the ready_queue 
// 7. perform context switch from the prev task to the new task

void tiny_scheduler() {

    // Here, we expect currTCB to have been set up by the start scdeduler function. 
    // TODO: There can be case like, where we don't do context switch if there are no more tasks.
    // FOR SUCH a condiiton, we need to implement a idle task. (inpirtaion: freeRTOS)

    // Time slice logic
    // time_slice++;

    // if a task delay is now ready, put it in the ready queue after popping it from the 
    // blocked due to time delay queue. 
    if (delayTaskReady()) {
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

    // We're moving up the stack so that sp points to R0 in our stack frame.
    // This is because, we simulate a exception return in this routine, which causes the CPU 
    // to pop registers R0-R3, R12, LC, xPSR from our stack.
    currTCB->sp += 8;   
    __set_PSP((uint32_t) currTCB->sp);
    __set_CONTROL(0x02);  // This means: nPRIV = 0, SPSEL = 1, FPCA = 0 : in CONTROL, only 3 LSB are used.
    __ISB();  // makes sure control register is fully applied 

    // We do EXC_RETURN now. The CPU kind of abandons this fucntion after we return by simulating this as exception return.
    __asm volatile(
        "MOV LR, #0xFFFFFFFD"  // Return to Thread mode using PSP
        "BX LR"
    );
}



__attribute__((naked))  void context_switch() {
    __asm volatile(
        // Save current context
        "MRS R0, PSP\n"          // R0 = PSP
        "STMDB R0!, {R4-R11}"    // store R4-R11 in PSP
        "LDR R1, =prevTCB\n"     // R0 = address of prevTCB sp variable (prevTCB = prevTCB->sp)
        "STR R0, [R1]"           // save PSP into current task's SP given by [prevTCB->sp variable]
        
        // Switch PSP
        "LDR R0, =currTCB\n"     // R0 = address of newTaskSP variable
        "LDR R0, [R0]\n"         // value of newTaskSP variable 
        "MSR PSP, R0\n"          // load the next Task SP into PSP: PSP = R0
        
        // restore context
        "LDMIA R0!, {R4-R11}\n"  // RePop the values into registers saved in the last context switch.
        "MSR PSP, R0"            // After poping from the PSP, we have to update the PSP as well.
        
        // Return
        "BX LR\n"
    );
}


bool rtos_wake_task_from_isr(TCB* task) {

    task->state = TASK_READY;

    // TODO: add to ready queue after popping from the waiting/blocked queue
    ReadyQueue_pushTCB(task);

    if (task->priority > currTCB->priority) {
        return true;
    }

    return false;
}

void rtos_request_context_switch(void) {
    // time_slice = 0; // reset time_slice
    prevTCB = currTCB;

    // if the curr task is still ready after it requests context switch then push it into 
    // the ready queue so that it still goes through the multi queue logic for selection 
    // as it may again be popped even though it was the one that requested the context 
    // switch.
    if (currTCB->state == TASK_READY) ReadyQueue_pushTCB(currTCB);


    // We get a task from ready queue. if there's none, we get the IDLE task 
    currTCB = ReadyQueue_popTCB();

    // This occurs when a HIGH priority queue only have 1 task in ready state.
    // we keep running this task until this task gets removed from ready state.
    if (currTCB == prevTCB) { 
        return;
    }

    // check which queue should the prev TCB go 
    if (prevTCB != NULL) { // which it would never be
        switch (currTCB->state) {
            case TASK_READY: 
                ReadyQueue_pushTCB(prevTCB);
                break;
            // TODO: case TASK_BLOCKED, 
            case TASK_TIME_DELAY:
                // IGNORE THE CURR_TCB IN THIS CASE AS THIS STATE IS ONLY SET UP BY THE 
                // taskDelay() which puts the currTCB in the delay queue.
                break;
        }
    } 

    // Trigger Context Switch: Pend the PendSV handler
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // Set PendSV to pending
}

