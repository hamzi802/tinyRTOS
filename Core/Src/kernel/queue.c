#include "queue.h"
#include <cassert>

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

// returns NULL if q is empty
TCB* dequeue(queue* q) {
    // queue is empty 
    if (q->head == q->tail) {
        return NULL;
    }

    TCB* tcb = q->tasks[q->head];
    q->head = (q->head + 1) % size;

    return tcb;
}

// returns NULL if queue is empty
TCB* getHead(queue* q) {
    return q->tasks[q->head];
}

// ----------------------------------------
// PRIORITY QUEUE FOR BLOCK TASKS: PRIORITY IS DELAY TICKS

void init_pq(TCBNodeLL* node_pool, int length, TCBNodeLL* free_head,
             TCBNodeLL* active_head) {
  memset(node_pool, 0, sizeof(TCBNodeLL) * length);

  // active list starts empty
  active_head->next = NULL;
  active_head->prev = NULL;

  // chain all nodes in free pool
  free_head->next = &node_pool[0];
  node_pool[0].prev = free_head;

  for (int i = 0; i < length - 1; i++) {
    node_pool[i].next = &node_pool[i + 1];
    node_pool[i + 1].prev = &node_pool[i];
  }
  node_pool[length - 1].next = NULL;
}

TCBNodeLL* pq_insert(TCBNodeLL* active_head, TCBNodeLL* free_head, TCB* tcb, uint32_t delay_ticks) {
  TCBNodeLL* node = free_head->next;
  if (node == NULL) return NULL;  // no space in pool.

  // detach from free_head
  free_head->next = node->next;
  if (node->next != NULL) node->next->prev  = free_head;

  // fill this node
  node->tcb = tcb;  
  node->delay_ticks = delay_ticks;

  // find insertion point based in the active list based on the priority i.e is the delay_ticks
  TCBNodeLL* curr = active_head;
  while (curr->next != NULL && curr->next->delay_ticks >= delay_ticks){
    curr = curr->next;
  }  


  node->next = curr->next;
  node->prev = curr;
  if (curr->next) curr->next->prev = node;
  curr->next = node;

  pqlength++;

  return node;
}

// active -> A -> B
TCBNodeLL* pq_dequeue(TCBNodeLL* active_head, TCBNodeLL* free_head) {
    TCBNodeLL* node = active_head->next;
    if (node == NULL) return NULL;

    // detech from active list
    active_head->next = node->next;
    if (node->next) node->next->prev = active_head;

    // add to free list
    node->next = free_head->next;
    node->prev = free_head;

    if (free_head->next) free_head->next->prev = node;
    free_head->next = node;

    pqlength--;

    return node; // caller reads node->tcb before it gets reused
}

TCBNodeLL* getHead(TCBNodeLL* active_head) {
    return active_head->next;
}