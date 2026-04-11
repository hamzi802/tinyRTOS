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

// BLOCK QUEUE

typedef struct TCBNodeLL TCBNodeLL;  

struct TCBNodeLL{
  TCB* tcb;
  TCBNodeLL* next;
  TCBNodeLL* prev;
  uint32_t delay_ticks;
};


void init_pq(TCBNodeLL* node_pool, int length, TCBNodeLL* free_head, TCBNodeLL* active_head);
TCBNodeLL* pq_insert(TCBNodeLL* active_head, TCBNodeLL* free_head, TCB* tcb, uint32_t delay_ticks);
TCBNodeLL* pq_dequeue(TCBNodeLL* active_head, TCBNodeLL* free_head);
TCBNodeLL* getHead(TCBNodeLL* active_head);