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

// BLOCK QUEUE: Linked List actually
typedef struct TCBNodeBLL{
    TCB* tcb;
    struct TCBNodeBLL* next;
    struct TCBNodeBLL* prev;
} TCBNodeBLL;

void BLL_init(TCBNodeBLL tcbLL[], uint32_t max_size, TCBNodeBLL* head);
TCBNodeBLL* BLL_insert(TCBNodeBLL* head, TCB* tcb, int* length);
TCBNodeBLL* BLL_delete(TCBNodeBLL* head, TCB* tcb, int *length);

// Delayed Priority Queue
typedef struct TCBNodeDPQ{
  TCB* tcb;
  struct TCBNodeDPQ* next;
  struct TCBNodeDPQ* prev;
  uint32_t delay_ticks;
} TCBNodeDPQ;


void init_pq(TCBNodeDPQ* node_pool, int length, TCBNodeDPQ* free_head, TCBNodeDPQ* active_head);
TCBNodeDPQ* pq_insert(TCBNodeDPQ* active_head, TCBNodeDPQ* free_head, uint32_t *pqlength, TCB* tcb, uint32_t delay_ticks);
TCBNodeDPQ* pq_dequeue(TCBNodeDPQ* active_head, TCBNodeDPQ* free_head, uint32_t *pqlength);
TCBNodeDPQ* pq_getHead(TCBNodeDPQ* active_head);

