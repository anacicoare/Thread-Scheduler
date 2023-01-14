#include "priority_queue.h"
#include <pthread.h>

#define MAX_NUM_THREADS 1024

struct params {
	unsigned int priority;
	so_handler *thread_func;
};


typedef struct conditions {
	pthread_mutex_t lock;
	pthread_cond_t ready;
	pthread_cond_t running;
	unsigned int ready_var;
} conditions_t;

typedef struct scheduler {
	unsigned int time_alloc;
	unsigned int nr_io;
	unsigned int nr_threads;
	tid_t *threads;
	conditions_t state;
	//tid_t turn_id;
	thread_info_t *current_thread;
	unsigned int thread_finished;
} scheduler_t;

pqueue *pq;

int init_error;
int initialized;
scheduler_t *my_scheduler;

int error_cases_for_init(unsigned int time_quantum, unsigned int io) {
	if(time_quantum <=0 || io < 0 || io > SO_MAX_NUM_EVENTS)
		return -1;
	return 0;
}

int so_init(unsigned int time_quantum, unsigned int io) {

	if(initialized == 1) {
		init_error = -1;
		return -1;
	}

	if (error_cases_for_init(time_quantum, io))
		return -1;

	// alloc mem for scheduler and its fields
	my_scheduler = calloc(1, sizeof(struct scheduler));
	if (my_scheduler == NULL)
		return -1;

	// fill scheduler fields
	my_scheduler->time_alloc = time_quantum;
	my_scheduler->nr_io = io;
	my_scheduler->state.ready_var = 0;
	//my_scheduler->turn_id = -1;
	my_scheduler->current_thread = NULL;
	my_scheduler->thread_finished = 0;

	// alloc memory for priority queue and init it
	pq = calloc(1, sizeof(pqueue));
	if (pq == NULL)
		return -1;
	pq->nr_elem = -1;

	// alloc memory for threads counter
	my_scheduler->nr_threads = 0;
	my_scheduler->threads = calloc(MAX_NUM_THREADS, sizeof(tid_t));
	if (my_scheduler->threads == NULL)
		return -1;

	// init mutex and condition variables
	int ret = pthread_mutex_init(&my_scheduler->state.lock, NULL);
	if (ret != 0)
		return -1;
	ret = pthread_cond_init(&my_scheduler->state.ready, NULL);
	if (ret != 0)
		return -1;
	ret = pthread_cond_init(&my_scheduler->state.running, NULL);
	if (ret != 0)
		return -1;

	//successfully initialized the scheduler
	initialized = 1;

	return 0;
}


int exists_thread(tid_t thread_id)
{
	int i;

	for (i = 0; i < my_scheduler->nr_threads; i++) {
		if (my_scheduler->threads[i] == thread_id)
			return 1;
	}

	return 0;
}

void put_thread_in_scheduler(thread_info_t* thread_info, struct scheduler* my_scheduler) {
	//my_scheduler->turn_id = thread_info->thread_id;
	my_scheduler->current_thread = thread_info;

}

void run_current_thread(struct scheduler* my_scheduler, int block) {
	pthread_cond_broadcast(&my_scheduler->state.running);
	pthread_mutex_unlock(&my_scheduler->state.lock);

	// current thread blocks
	if (block == 1 && my_scheduler->current_thread->thread_id != pthread_self()) {
		// current thread waits to be planned
		pthread_mutex_lock(&my_scheduler->state.lock);
		while (my_scheduler->current_thread->thread_id != pthread_self())
			pthread_cond_wait(&my_scheduler->state.running, &my_scheduler->state.lock);
		pthread_mutex_unlock(&my_scheduler->state.lock);
	}
}

void replace_with_higher_priority_thread(struct scheduler* my_scheduler) {
	thread_info_t *best = peek(pq);
	if (best != NULL)
		put_thread_in_scheduler(best, my_scheduler);
}

void get_next_thread(struct scheduler* my_scheduler, int block) {
	thread_info_t *best = peek(pq);
	// no threads found, so we run this thread again
	if (best == NULL) {
		my_scheduler->current_thread->time_available = my_scheduler->time_alloc;
		run_current_thread(my_scheduler, block);
	}

	// if we found a higher priority thread
	if (best->priority > my_scheduler->current_thread->priority) {
		my_scheduler->current_thread->time_available = my_scheduler->time_alloc;
		//my_scheduler->turn_id = best->thread_id;
		my_scheduler->current_thread = best;
	} else {
		// replace with next thread
		thread_info_t *another = peek(pq);

		my_scheduler->current_thread->time_available = my_scheduler->time_alloc;
		//my_scheduler->turn_id = another->thread_id;
		my_scheduler->current_thread = another;
	}	
}

void first_schedule(struct scheduler* my_scheduler) {
	thread_info_t* thread_info = peek(pq);
	put_thread_in_scheduler(thread_info, my_scheduler);

	// announce that thread has been scheduled
	pthread_cond_broadcast(&my_scheduler->state.running);
	pthread_mutex_unlock(&my_scheduler->state.lock);
}

void schedule(int block)
{
	// lock mutex to analize running thread
	pthread_mutex_lock(&my_scheduler->state.lock);

	// no threads in the queue
	if (is_empty(pq) == 1) {
		return;
	}

	// first schedule
	if (my_scheduler->current_thread == NULL) {
		first_schedule(my_scheduler);
		return;
	}

	// if the current thread finished
	if (my_scheduler->thread_finished == 1) {
		my_scheduler->thread_finished = 0;
		get_next_thread(my_scheduler, block);

	// if the current thread finished its quantum
	} else if (my_scheduler->current_thread->time_available == 0)
		get_next_thread(my_scheduler, block);

	// if the quantum did not expire but we have a higher priority thread
	else replace_with_higher_priority_thread(my_scheduler);

	run_current_thread(my_scheduler, block);
}

void announce_thread_ready(struct scheduler* my_scheduler) {
	pthread_mutex_lock(&my_scheduler->state.lock);
	my_scheduler->state.ready_var = 1;
	pthread_cond_signal(&my_scheduler->state.ready);
	pthread_mutex_unlock(&my_scheduler->state.lock);
}

void *start_thread(void *p)
{
	struct params func_params = *(struct params *) p;
	unsigned int prio = func_params.priority;
	so_handler *thread_func = func_params.thread_func;

	// announce that current thread is ready for planning
	announce_thread_ready(my_scheduler);

	// wait for this thread to be planned
	pthread_mutex_lock(&my_scheduler->state.lock);
	while (my_scheduler->current_thread->thread_id != pthread_self())
		pthread_cond_wait(&my_scheduler->state.running, &my_scheduler->state.lock);
	pthread_mutex_unlock(&my_scheduler->state.lock);

	// call function from parameters
	thread_func(prio);

	// thread finished
	my_scheduler->thread_finished = 1;
	pop(pq, my_scheduler->current_thread);
	schedule(0);

	return NULL;
}

tid_t so_fork(so_handler *func, unsigned int priority)
{
	if (func == NULL || priority < 0 || priority > SO_MAX_PRIO)
		return INVALID_TID;

	// init thread struct
	thread_info_t *new_thread = calloc(1, sizeof(thread_info_t));
	if (new_thread == NULL)
		return INVALID_TID;

	new_thread->priority = priority;
	new_thread->thread_func = func;
	new_thread->time_available = my_scheduler->time_alloc;
	new_thread->waiting_io = 0;
	new_thread->device_io = -1;

	// parameters send to thread's routine function
	struct params p;
	p.priority = priority;
	p.thread_func = func;

	tid_t thread_id;

	// create new thread to execute start_thread function
	int ret = pthread_create(&thread_id, NULL, &start_thread, (void *) &p);
	if (ret != 0)
	 	return INVALID_TID;

	// add thread to the scheduler
	new_thread->thread_id = thread_id;
	push(pq, new_thread, priority);
	my_scheduler->threads[my_scheduler->nr_threads] = thread_id;
	my_scheduler->nr_threads++;

	// barrier to let only the desired thread pass
	pthread_mutex_lock(&my_scheduler->state.lock);
	while (my_scheduler->state.ready_var == 0)
		pthread_cond_wait(&my_scheduler->state.ready, &my_scheduler->state.lock);
	my_scheduler->state.ready_var = 0;
	pthread_mutex_unlock(&my_scheduler->state.lock);

	// reduce time quantum of the current thread, if it was added to the queue
	if (exists_thread(pthread_self()) == 1)
		my_scheduler->current_thread->time_available--;

	schedule(1);

	return thread_id;
}

int so_wait(unsigned int io)
{
	my_scheduler->current_thread->time_available--;

	if (io < 0 || io >= my_scheduler->nr_io) {
		schedule(1);
		return -1;
	}

	my_scheduler->current_thread->waiting_io = 1;
	my_scheduler->current_thread->device_io = io;

	schedule(1);

	return 0;
}

int so_signal(unsigned int io)
{
	my_scheduler->current_thread->time_available--;

	if (io < 0 || io >= my_scheduler->nr_io) {
		schedule(1);
		return -1;
	}

	int woken_up_threads = wake_up_all(pq, io);

	schedule(1);
	return woken_up_threads;
}

void so_exec(void)
{
	my_scheduler->current_thread->time_available--;

	schedule(1);
}

void so_end(void)
{
	if (init_error == 1 || initialized == 0)
		return;

	// asteapta toate thread-urile
	for (int i = 0; i < my_scheduler->nr_threads; i++)
		pthread_join(my_scheduler->threads[i], NULL);

	destroy_queue(pq);
	free(pq);
	free(my_scheduler->threads);
	pthread_mutex_destroy(&my_scheduler->state.lock);
	pthread_cond_destroy(&my_scheduler->state.ready);
	pthread_cond_destroy(&my_scheduler->state.running);
	free(my_scheduler);

	initialized = 0;
	init_error = 0;
}