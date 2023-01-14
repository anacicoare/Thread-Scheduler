#include <stdio.h>
#include <stdlib.h>
#include "so_scheduler.h"

typedef struct thread_info {
tid_t thread_id;
unsigned int priority;
so_handler *thread_func;
int time_available;
int waiting_io;
int device_io;
} thread_info_t;


typedef struct node {
	thread_info_t* data;
	int priority;
} node_t;

typedef struct pqueue {
	node_t nodes[1024];
	int nr_elem;
} pqueue;

int is_empty(pqueue *pq);
thread_info_t *peek(pqueue *pq);
void push(pqueue *pq, thread_info_t *thread_info, int priority);
void pop(pqueue *pq, thread_info_t *thread_info);
int wake_up_all(pqueue *pq, int io);
void destroy_queue(pqueue *pq);