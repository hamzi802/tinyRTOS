#pragma once

// expects a queue like
typedef struct {
    TCB* *tasks; // array of TCB pointers.
    volatile uint32_t head, tail;
    uint32_t size;
} queue;

void init_queue(queue* q, TCB** tcbArr, uint32_t size);
bool enqueue(queue* q, TCB* tcb); 
TCB* dequeue(queue* q); 
TCB* getHead(queue* q);
