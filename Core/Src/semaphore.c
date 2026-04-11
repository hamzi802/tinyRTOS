#include "semaphore.h"
#include "stm32f4xx_it.h" // Required for __disable_irq() and __enable_irq()

/* =========================================
 * BINARY SEMAPHORE IMPLEMENTATION
 * ========================================= */

void bin_sem_init(BinarySemaphore* s, uint8_t initial_value) {
    // Ensure the initial value is strictly 0 or 1
    if (initial_value > 0) {
        s->count = 1;
    } else {
        s->count = 0;
    }
}

void bin_sem_wait(BinarySemaphore* s) {
    while (1) {
        __disable_irq(); // Enter Critical Section
        
        if (s->count > 0) {
            s->count = 0;   // Take the semaphore
            __enable_irq(); // Exit Critical Section
            return;         // Successfully taken, exit function
        }
        
        __enable_irq(); // Semaphore unavailable, enable interrupts so RTOS can switch tasks
    }
}

void bin_sem_signal(BinarySemaphore* s) {
    __disable_irq();
    s->count = 1; // Release the semaphore (cap at 1)
    __enable_irq();
}

int bin_sem_trywait(BinarySemaphore* s) {
    int success = 0;
    
    __disable_irq();
    if (s->count > 0) {
        s->count = 0;
        success = 1; // Successfully took it
    }
    __enable_irq();
    
    return success; // Returns 1 if taken, 0 if it was empty
}

/* =========================================
 * COUNTING SEMAPHORE IMPLEMENTATION
 * ========================================= */

void count_sem_init(CountingSemaphore* s, int32_t initial_value, int32_t max_value) {
    s->count = initial_value;
    s->max_count = max_value;
}

void count_sem_wait(CountingSemaphore* s) {
    while (1) {
        __disable_irq(); // Enter Critical Section
        
        if (s->count > 0) {
            s->count--;     // Take one token from the count
            __enable_irq(); // Exit Critical Section
            return;
        }
        
        __enable_irq(); // None available, wait for another task to signal
    }
}

void count_sem_signal(CountingSemaphore* s) {
    __disable_irq();
    
    // Only increment if we haven't hit the maximum limit
    if (s->count < s->max_count) {
        s->count++; // Return a token to the count
    }
    
    __enable_irq();
}

int count_sem_trywait(CountingSemaphore* s) {
    int success = 0;
    
    __disable_irq();
    if (s->count > 0) {
        s->count--;
        success = 1;
    }
    __enable_irq();
    
    return success;
}
