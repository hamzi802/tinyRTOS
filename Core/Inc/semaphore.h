#ifndef SEMAPHORE_H
#define SEMAPHORE_H
#include <stdint.h>
/*
 * BINARY SEMAPHORE
 * Acts like a boolean flag (0 or 1).
 */
typedef struct {
    volatile uint8_t count; 
} BinarySemaphore;

void bin_sem_init(BinarySemaphore* s, uint8_t initial_value);
void bin_sem_wait(BinarySemaphore* s);
void bin_sem_signal(BinarySemaphore* s);
int  bin_sem_trywait(BinarySemaphore* s);


/*
 * COUNTING SEMAPHORE
 * Keeps track of multiple available resources.
 */
typedef struct {
    volatile int32_t count;
    volatile int32_t max_count; // Optional, but prevents overflow bugs
} CountingSemaphore;

void count_sem_init(CountingSemaphore* s, int32_t initial_value, int32_t max_value);
void count_sem_wait(CountingSemaphore* s);
void count_sem_signal(CountingSemaphore* s);
int  count_sem_trywait(CountingSemaphore* s);

#endif // SEMAPHORE_H
