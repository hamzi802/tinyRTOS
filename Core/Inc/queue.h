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

typedef struct TCBNodeDPQ TCBNodeDPQ;  

struct TCBNodeDPQ{
  TCB* tcb;
  TCBNodeDPQ* next;
  TCBNodeDPQ* prev;
  uint32_t delay_ticks;
};


void init_pq(TCBNodeDPQ* node_pool, int length, TCBNodeDPQ* free_head, TCBNodeDPQ* active_head);
TCBNodeDPQ* pq_insert(TCBNodeDPQ* active_head, TCBNodeDPQ* free_head, uint32_t pqlength, TCB* tcb, uint32_t delay_ticks);
TCBNodeDPQ* pq_dequeue(TCBNodeDPQ* active_head, TCBNodeDPQ* free_head, uint32_t pqlength);
TCBNodeDPQ* pq_getHead(TCBNodeDPQ* active_head);