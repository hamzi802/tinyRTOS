#include "kernel.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal_cortex.h"
#include <cassert>
#include <stdint.h>


TCB* currTCB = NULL;
TCB* nextTCB = NULL;

uint32_T time_slice = 0;


// TODO: 1. GET RID OF THESE TaskSP by putting the sp as the first field of the TCB. 
//       2. LEARN function pointers correctly to load the task routine for the first time when the scheduler fires up. 

// When the scheduler has already ran.
// 1. Set the currTask to the nextTask (the previous task).
// 1. get the next task
// 2. Push the current task to the queue.
// 3. Perform the context switch.

// When the scheduler runs for the first time.
// 1. Get a task's TCB from the ready_queue.
// 2. Run that task's routine.

void tiny_scheduler() {

    // Here, we expect currTCB to have been set as its set up by the start scdeduler function. 
    // TODO: There can be case like, where we don't do context switch if there are no more tasks.
    // if (currTCB == nextTCB || nextTCB == NULL) {
    //     return;
    // }

    // Time slice logic
    time_slice++;

    if (time_slice != 5) {
        return;
    }

    time_slice = 0; // reset time_slice
    currTCB = nextTCB;


    // We get a task from ready queue. 
    if ((nextTCB = ReadyQueue_popTCB()) == NULL) {
        // no task in any ready queue
        assert(false); // TODO
    }

    if (currTCB != NULL) ReadyQueue_pushTCB();

    // Trigger Context Switch: Pend the PendSV handler
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // Set PendSV to pending
}

// This is run by the main file or by the kernel_init();
// It simulates returning from an exception. 
void start_scheduler() {
    if ((currTCB = ReadyQueue_popTCB()) == NULL) {
        // no task in any ready queue
        assert(false); // TODO
    }

    // We're moving up the stack so that sp points to R0 in our stack frame.
    // This is because, we simulate a exception return in this routine, which causes the CPU 
    // to pop registers R0-R3, R12, LC, xPSR from our stack.
    currTCB->sp += 8;   
    __set_PSP((uint32_t) currTCB->sp);
    __set_CONTROL(0x02)  // This means: nPRIV = 0, SPSEL = 1, FPCA = 0 : in CONTROL, only 3 LSB are used.
    __ISB();  // makes sure control register is fully applied 

    // We do EXC_RETURN now. The CPU kinds of abandons this fucntion after we return by simulating this as exception return.
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
        "LDR R1, =currTCB\n"  // R0 = address of currTaskSP variable
        "STR R0, [R1]"           // save PSP into current task's SP given by [currTaskSP variable]
        
        // Switch PSP
        "LDR R0, =nextTCB\n"   // R0 = address of newTaskSP variable
        "LDR R0, [R0]\n"         // value of newTaskSP variable 
        "MSR PSP, R0\n"          // load the next Task SP into PSP: PSP = R0
        
        // restore context
        "LDMIA R0!, {R4-R11}\n"  // RePop the values into registers saved in the last context switch.
        "MSR PSP, R0"            // After poping from the PSP, we have to update the PSP as well.
        
        // Return
        "BX LR\n"
    );
}