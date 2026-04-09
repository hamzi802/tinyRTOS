#include "kernel.h"
#include "stm32f401xc.h"


uint32_t* currTaskSP; // address of SP of the current task
uint32_t* nextTaskSP; // address of SP of the next task 




void tiny_scheduler() {

    // Pend the PendSV handler
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // Set PendSV to pending

    // The PendSV handler performs the context switch
}






__attribute__((naked))  void context_switch() {
    __asm volatile(
        // Save current context
        "MRS R0, PSP\n"          // R0 = PSP
        "STMDB R0!, {R4-R11}"    // store R4-R11 in PSP
        "LDR R1, =currTaskSP\n"  // R0 = address of currTaskSP variable
        "STR R0, [R1]"           // save PSP into current task's SP given by [currTaskSP variable]
        
        // Switch PSP
        "LDR R0, =newTaskSP\n"   // R0 = address of newTaskSP variable
        "LDR R0, [R0]\n"         // value of newTaskSP variable 
        "MSR PSP, R0\n"          // load the next Task SP into PSP: PSP = R0
        
        // restore context
        "LDMIA R0!, {R4-R11}\n"  // RePop the values into registers saved in the last context switch.
        "MSR PSP, R0"            // After poping from the PSP, we have to update the PSP as well.
        
        // Return
        "BX LR\n"
    );
}