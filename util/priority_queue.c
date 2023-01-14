#include "priority_queue.h"


int is_empty(pqueue *pq)
{
	if (pq->nr_elem == -1)
		return 1;
	return 0;
}

thread_info_t *peek(pqueue *pq)
{
	if (is_empty(pq))
		return NULL;

	int i;

	// queue with 1 element
	if (pq->nr_elem == 0) {
		return pq->nodes->data;
	}

	for (i = 0; i <= pq->nr_elem; i++)
		if (pq->nodes[i].data->waiting_io == 0)
			return pq->nodes[i].data;

	return NULL;
}

void push(pqueue *pq, thread_info_t *thread_info, int priority)
{
	int i;

	
	if (is_empty(pq)) {
		pq->nr_elem = 0;
		pq->nodes[0].data = thread_info;
		pq->nodes[0].priority = priority;
	} else {

		for (i = pq->nr_elem; priority > pq->nodes[i].priority && i >= 0; i--)
			pq->nodes[i + 1] = pq->nodes[i];

		//if inserting the first element
		if(i == 0 && priority > pq->nodes[i].priority) {
			pq->nodes[i].data = thread_info;
			pq->nodes[i].priority = priority;
  
			pq->nr_elem++;
			return;
		}

		// insert in the middle / end
		pq->nodes[i + 1].data = thread_info;
		pq->nodes[i + 1].priority = priority;
		
		// modify tail
		pq->nr_elem++;

	}
}

void pop(pqueue *pq, thread_info_t *thread_info) {
	int i, j;

	if (is_empty(pq)) {
		return;
	}

	for (i = 0; i <= pq->nr_elem; i++) {
		if (pq->nodes[i].data == thread_info) {
			free(pq->nodes[i].data);

			// shift all elements to the left
			for (j = i; j <= pq->nr_elem; j++)
			{
				pq->nodes[j] = pq->nodes[j + 1];
				if (j == pq->nr_elem)
					break;
			}
			break;
		}
			
	}

		// update tail
		pq->nr_elem--;
}


int wake_up_all(pqueue *pq, int io)
{
	int nr = 0;
	int i;

	if (is_empty(pq))
		return 0;

	for (i = 0; i < pq->nr_elem; i++) {
		if (pq->nodes[i].data->waiting_io == 1 && pq->nodes[i].data->device_io == io) {
			nr++;
			pq->nodes[i].data->waiting_io = 0;
		}
	}

	return nr;
}

void destroy_queue(pqueue *pq)
{
	int i;

	if (is_empty(pq)) {
		return;
	}

	for (i = 0; i < pq->nr_elem; i++)
		free(pq->nodes[i].data);
}