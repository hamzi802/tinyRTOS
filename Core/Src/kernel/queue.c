#include "queue.h"

// Our QUEUE Convention
// Tail -> write
// Head -> read

// NOTE

// if (head == tail), then the queue can either be full or empty.
// To solve, we can either have a variable COUNT so that COUNT == MAXSIZE is full
// and COUNT == 0 is empty however, to save on that variable

// queue is empty: head == tail 
// queue is full: (head - tail + MAX_SIZE) % MAX_SIZE == 1
// But this comes at the cost that we consider one slot left as "full"
// But this approach is thread safe as it helps avoid COUNT which is changed
// by both enqueue and dequeue. 



void init_queue(queue* q, TCB** tcbArr, uint32_t size) {
    q->head = 0;
    q->tail = 0;
    q->size = size;
    q->tasks = tcbArr;
}

bool enqueue(queue* q, TCB* tcb) {
    uint32_t next = (q->tail + 1) % size;

    // queue is full
    if (next == q->head) {
        return false;
    }

    q->tasks[q->tail] = tcb;
    // DMB only needed when ISR <--> main loop communication
    __DMB(); // ensures the next isntruction runs only after the previous memory operations finish
    q->tail = next;

    return true;
}

TCB* dequeue(queue* q) {
    // queue is empty 
    if (q->head == q->tail) {
        return NULL;
    }

    TCB* tcb = q->tasks[q->head];
    q->head = (q->head + 1) % size;

    return tcb;
}

