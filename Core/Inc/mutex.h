#ifndef MUTEX_H
#define MUTEX_H
#include <stdint.h>
typedef struct {
    volatile uint8_t locked;
    volatile int32_t owner;
} Mutex;
void mutex_init(Mutex* m);
void mutex_lock(Mutex* m, int32_t taskId);
void mutex_unlock(Mutex* m, int32_t taskId);
int mutex_trylock(Mutex* m, int32_t taskId);
#endif
