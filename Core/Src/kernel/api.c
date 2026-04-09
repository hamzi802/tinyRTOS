#include "kernel.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.h"


TCB* ready_tasks[MAX_TASKS];

queue ready_queue;

init_queue(&ready_queue, ready_tasks, MAX_TASKS);

void newTask(char* task_name, uint32_t task_routine) {
    TCB tcb = {
        .tid = 0,
        .task_name = task_name,
        .sp = NULL,
        .taskStack = {.data = {0}} // initialize stack to zero
    };

    // we do this so that sp points to top of the stack. 

    // type conversion done because tcb.taskStack.data is uint8. Stack size is also in bytes.
    tcb.sp = (uint32_t*) (tcb.taskStack.data + STACK_SIZE);  



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
    tcb.sp = tcb.sp - 16;

    // set the PC to address of the routine of the task we want to run
    // In the stack, from top, the 15th register is PC so 

    *(tcb.sp + 14) = task_routine;

    // set the xPSR (the 16th register) to 0x01000000
    // 0x01000000 basically sets the Thumb Mode xPSR bit to 1 so that 
    // CPU uses the Thumb mode.
    *(tcb.sp + 15) = 0x01000000;


    // Finally, insert this into our ready tasks queue.
}