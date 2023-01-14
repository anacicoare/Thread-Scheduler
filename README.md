# Thread-Scheduler

### Organization

- the idea is to implement a thread scheduler in user-space as a shared (.so) library
- the solution is based on a C program which stimulates a preemptive thread scheduler for UNIX based systems, in a single processor system, using a Round Robin with priorities algorithm.

### Implementation details

- this shared library offers a common interface to all threads that have to be planned. The exported functions are the following:

 - so_init(quantum, io) - initializes the scheduler
 - so_end() - frees the scheduler structure
 - so_fork(handler, priority) - creates a new thread in kernel space and adds all required informations in the planner's priority queue. As soon as the thread started, it notifies the parent that it is ready for planning. The parent wakes up and chooses the best thread and runs it. All the other threads are waiting to be chosen.
 - so_exec() - stimulates the execution of a instruction, decreasing the quantum
 - so_wait(event/io) - marks the current thread as in use for an I/O event and does not allow the thread to be planned, until another thread signals it for the same event
 - so_signal(event/io) - wakes up all threads waiting on the specified I/O

- the schedule function is based on a priroty queue, which will always keep the best thread at the top (as long as that thread is waiting to be scheduled)
- the current thread's block was done using a mutex - condition variable - mutex construction in which the thread calls pthread_mutex_lock/EnterCriticalSection, waits for pthread_cond_wait/SleepConditionVariableCS, and then frees the mutex when the condition is true using pthread_mutex_unlock/LeaveCriticalSection
- similarly, the signaling was done using pthread_cond_broadcast


### How to run

- simply clone the repository and type 
```bash
    cd util && make
```
- and so, the checked will start