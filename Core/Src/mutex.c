#include "mutex.h"
#include "stm32f4xx_it.h" // Required for __disable_irq() and __enable_irq()

void mutex_init(Mutex* m) {
    m->locked = 0;
    m->owner = -1;
}

void mutex_lock(Mutex* m, int32_t taskId) {
    while (1) {
        __disable_irq(); // Pause RTOS task switching
        
        if (m->locked == 0) {
            m->locked = 1;
            m->owner = taskId;
            __enable_irq(); // Resume RTOS
            return;         // We have the lock, exit the function
        }
        
        __enable_irq(); // Resume RTOS so other tasks can run while we wait
    }
}

void mutex_unlock(Mutex* m, int32_t taskId) {
    __disable_irq(); // Pause RTOS
    
    // Only the task that locked it is allowed to unlock it
    if (m->owner == taskId) {
        m->locked = 0;
        m->owner = -1;
    }
    
    __enable_irq(); // Resume RTOS
}

int mutex_trylock(Mutex* m, int32_t taskId) {
    int success = 0;
    
    __disable_irq(); // Pause RTOS
    if (m->locked == 0) {
        m->locked = 1;
        m->owner = taskId;
        success = 1; // Successfully got the lock
    }
    __enable_irq(); // Resume RTOS
    
    return success; // Returns 1 if locked, 0 if it was already busy
}
