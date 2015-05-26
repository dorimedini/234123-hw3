
#include "threadPool.h"
#include <stdlib.h>
		

/**
 * Task object
 *
 * These are the objects stored in the thread pool's task queue
 */
typedef struct task_obj {
	void (*func)(void *);	// The function object
	void* param;			// The function's parameter
} Task;

/**
 * Returns a string representation of the thread pool's state
 */
char* state_string(State state) {
	switch(state) {
		case ALIVE: return "ALIVE";
		case DO_ALL: return "DO_ALL";
		case DO_RUN: return "DO_RUN";
	}
	return NULL;
}
 

/**
 * The function sent to all threads in the thread pool
 */
void* thread_func(void*);




/**
 * Create the thread pool.
 *
 * Pretty simple, just make sure the threads we create can't do anything
 * until we're done.
 */
ThreadPool* tpCreate(int num) {
	
	// Init:
	if (num < 0)													// Sanity check
		return NULL;
	ThreadPool* tp = (ThreadPool*)malloc(sizeof(ThreadPool));		// Create the pool
	if (!tp)														// Make sure it worked
		return NULL;
	tp->threads = (pthread_t*)malloc(sizeof(pthread_t)*num);		// Create the thread array
	if (!tp->threads) {												// Make sure it worked
		free(tp);
		return NULL;
	}
#if HW3_DEBUG
	tp->tids = (int*)malloc(sizeof(int)*num);
	if (!tp->tids) {
		free(tp->threads);
		free(tp);
		return NULL;
	}
	int tid_i;
	for (tid_i=0; tid_i<num; ++tid_i)
		tp->tids[tid_i]=0;
#endif
	
	// Locks and things
	pthread_mutexattr_init(&tp->mutex_type);
	pthread_mutexattr_settype(&tp->mutex_type,PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&tp->task_lock, &tp->mutex_type);
	pthread_mutex_init(&tp->state_lock, &tp->mutex_type);
	pthread_cond_init(&tp->queue_not_empty_or_dying, NULL);
	
	// Basic fields
	tp->N = num;
	tp->state = ALIVE;
	tp->tasks = osCreateQueue();
	
	// Create threads!
	// No need to lock anything, the threads are now allowed
	// to read the task queue and the state anyway - neither
	// of them are going to be touched here.
	int i,ret;
	for (i=0; i<num; ++i) {
		ret = pthread_create(tp->threads + i, NULL, thread_func, (void*)tp);
		PRINT("Creating thread %d, return value: %d\n",i+1,ret);
	}
	
	// Return the ThreadPool
	return tp;
	
}




/**
 * Destroys the thread pool.
 *
 * 1. Lock the destruction lock
 * 2. Check the state. If we're dying, unlock and return
 * 3. Change the state
 * 4. Broadcast to all threads waiting for tasks: if there's nothing now, there never
 *    will be!
 * 5. Unlock the tasks lock (to allow the threads to do their thing)
 * 6. Wait for all threads to finish
 * 7. Destroy all fields of the pool
 */
void tpDestroy(ThreadPool* tp, int should_wait_for_tasks) {
	
	if (!tp)												// Sanity check
		return;
	
	// Lock the state lock. This is OK, because we're going to
	// unlock it before we lock the task lock so order is preserved
	pthread_mutex_lock(&tp->state_lock);
	if (tp->state != ALIVE) {								// Destruction already in progress.
		pthread_mutex_unlock(&tp->state_lock);
		return;
	}
	tp->state = should_wait_for_tasks ? DO_ALL : DO_RUN;	// Enter destroy mode
	pthread_mutex_unlock(&tp->state_lock);
	
	// Lock the task lock so we know any thread that will wait for the broadcast is in
	// fact waiting (this prevents deadlocks where a thread will wait for a signal forever)
	PRINT("State changed to %s, broadcasting...\n", should_wait_for_tasks ? "DO_ALL" : "DO_RUN");
	pthread_mutex_lock(&tp->task_lock);
	pthread_cond_broadcast(&tp->queue_not_empty_or_dying);
	PRINT("... done. Unlocking the task lock.\n");
	pthread_mutex_unlock(&tp->task_lock);
	
	// Wait for all threads to exit (this could take a while)
	int i;
	for (i=0; i<tp->N; ++i) {
		PRINT("Waiting for T%2d to finish (tid=%d)...\n", i+1, tp->tids[i]);
		pthread_join(tp->threads[i], NULL);
		PRINT("T%2d (tid=%d) done.\n", i+1, tp->tids[i]);
	}
	PRINT("All threads done! Locking the task lock and destroying the task queue...\n");
	
	// Cleanup!
	// We can still lock here, but no need - no more threads exist and
	// tpInsertTask won't do anything because the state isn't ALIVE
	while (!osIsQueueEmpty(tp->tasks)) {
		Task* t = (Task*)osDequeue(tp->tasks);
		free(t);
	}
	osDestroyQueue(tp->tasks);
	PRINT("Done. Unlocking the task queue\n");
	
	// Locks:
	PRINT("Doing thread pool cleanup...\n");
	pthread_mutex_destroy(&tp->task_lock);
	pthread_mutex_destroy(&tp->state_lock);
	pthread_mutexattr_destroy(&tp->mutex_type);
	pthread_cond_destroy(&tp->queue_not_empty_or_dying);
	
	// Last cleanup, and out:
#if HW3_DEBUG
	free(tp->tids);
#endif
	free(tp->threads);
	free(tp);
	PRINT("Done destroying thread pool.\n");
	return;
}




/**
 * Add task.
 *
 * Basically:
 * 1. Allocate memory for the new task. This should be done first because it could take
 *    a while, and we shouldn't keep the thread pool locked while we're at it.
 *    If the pool is dying this is a waste of time, but the user shouldn't be calling
 *    this function on a dying pool anyway
 * 2. Lock
 * 3. Read the state. If we're dying, unlock, free, and return
 * 4. Add the task
 * 5. send a signal
 * 6. Unlock
 */
int tpInsertTask(ThreadPool* tp, void (*func)(void *), void* param) {
	
	// Try creating the task now, before locking the pool
	Task* t = (Task*)malloc(sizeof(Task));
	if (!t) {
		return -1;
	}
	t->func = func;
	t->param = param;
	
	// Read the state.
	// It's important to lock the task lock first, to preserve the order,
	// and s.t. the pool isn't destroyed after we unlock the state lock
	// and before we lock the task lock...
	pthread_mutex_lock(&tp->task_lock);
	pthread_mutex_lock(&tp->state_lock);
	if (tp->state != ALIVE) {	// If the thread pool is dying,
		pthread_mutex_unlock(&tp->state_lock);
		pthread_mutex_unlock(&tp->task_lock);
		free(t);
		return -1;				// and return indication of destruction.
	}
	
	// Releasing the state lock at this point may  be dangerous: we want the task to be in
	// the queue before it's possible to change the state from ALIVE.
	// If, for example, we release the state lock here, we risk a context switch to a thread
	// that calls tpDestroy + context switches favoring the pool's threads causing them all
	// to exit + switch back to the destroyer, we could actually add a task to a dead pool.
	// HOWEVER:
	// This can't happen, because to signal to the threads the tpDestroy function needs the
	// task lock (which we still have) and threads will check if the queue is empty in the
	// future because the state was ALIVE when we started tpInsertTask, and we have the task
	// lock!
	pthread_mutex_unlock(&tp->state_lock);
	
	// Editing the task queue now.
	// There's no danger of deadlock with thread acquiring this lock: If a thread isn't waiting
	// for a signal then it will check if the queue is empty in the near future (or in the far
	// future, if it's in the middle of a long task)
	PRINT("Adding a task, lock is locked\n");
	osEnqueue(tp->tasks,(void*)t);
	
	// Signal before releasing the lock - make a thread wait for the lock.
	pthread_cond_signal(&tp->queue_not_empty_or_dying);	
	PRINT("Task added, signal given, lock is locked\n");
	
	// Unlock. After this, the pool should be destroyable and tasks should be runnable
	pthread_mutex_unlock(&tp->task_lock);
	PRINT("Task added, signal given, lock is unlocked\n");
	
	return 0;
	
}




/**
 * The function passed to created threads.
 *
 * NOTE: is_empty(queue) is called a lot: It should be noted that we
 * must make sure it's only called when we have the queue lock!
 *
 * 1. Lock the task queue. We're using a condition lock, so we'll
 *    give up the lock until there is a task to run OR the tpDestroy
 *    function sent a broadcast to all threads that they should clean
 *    up
 * 2. Wait for the signal (task inserted, or tp being destroyed)
 * 3. Now that the queue is locked, check the destruction state
 * 4. If we're ALIVE, that means pool->queue IS NOT EMPTY (otherwise we
 *    would still be in the while loop), so we can just dequeue a task
 *    and run it (remember to unlock before running!)
 * 5. If we're DO_ALL, it's like ALIVE but first check if there's
 *    something to run (unlike the ALIVE state, we don't know for sure).
 *    If there is, run it; otherwise, terminate (no more tasks will come)
 * 6. If we're DO_RUN, terminate. Don't take another task, leave them to rot
 * 7. Rinse and repeat
 */
void* thread_func(void* void_tp) {
	
	int pid = TID();
	
	// Some useful variables
	Task* t;
	ThreadPool* tp = (ThreadPool*)void_tp;
	
#if HW3_DEBUG
	// Initialize tp->tids
	pthread_t self = pthread_self();
	int thread_i;
	for (thread_i=0; thread_i<tp->N; ++thread_i)
		if (pthread_equal(tp->threads[thread_i],self)) {
			tp->tids[thread_i]=pid;
			break;
		}
#endif
	PRINT("Thread %d started it's function\n",pid);
	
	// Main thread task
	while(1) {
		
		// Get the initial state and the task lock, when we need it (task to do or we're dying).
		// Lock the locks in there RESOURCE ORDER (first the task lock, then the state lock)
		pthread_mutex_lock(&tp->task_lock);										// This is OK because during INIT, we don't lock the task queue (after its creation)
		pthread_mutex_lock(&tp->state_lock);
		PRINT("Thread %d locked the task queue\n",pid);
		while (osIsQueueEmpty(tp->tasks) && tp->state == ALIVE) {				// Wait for a task OR the destruction of the pool
			PRINT("Thread %d started waiting for a signal\n",pid);
			pthread_mutex_unlock(&tp->state_lock);
			pthread_cond_wait(&tp->queue_not_empty_or_dying,&tp->task_lock);	// Either one gives a signal
			pthread_mutex_lock(&tp->state_lock);
			PRINT("Thread %d got the signal and locked the lock\n",pid);
		}
		PRINT("Thread %d got out of the while() loop, state==%s\n",pid,state_string(read_state(tp)));
		
		// Do different things depending on the state of the pool,
		// but the first thing should always be to unlock the state lock.
		switch(tp->state) {
			case ALIVE:											// If we're not dying, take a task and do it.
				pthread_mutex_unlock(&tp->state_lock);
				t = (Task*)osDequeue(tp->tasks);
				pthread_mutex_unlock(&tp->task_lock);
				PRINT("Thread %d doing it's task\n",pid);
				t->func(t->param);
				free(t);
				break;
			case DO_ALL:										// If we're dying, but we should clean up the queue:
				pthread_mutex_unlock(&tp->state_lock);
				if (!osIsQueueEmpty(tp->tasks)) {				// THIS TEST IS NOT USELESS! We may have got here
					t = (Task*)osDequeue(tp->tasks);			// via a broadcast() call from tp_destroy and the
					pthread_mutex_unlock(&tp->task_lock);		// state may be DO_ALL but is_empty() may be true...
					PRINT("Thread %d doing it's task\n",pid);	// Thus, the while() loop terminated and we got here.
					t->func(t->param);
					free(t);
				}
				else {											// If we're here, there are no more tasks to dequeue!
					pthread_mutex_unlock(&tp->task_lock);		// As we're being destroyed anyway, exit.
					PRINT("Thread %d unlocked the lock and returning\n",pid);
					return NULL;
				}
				break;
			case DO_RUN:										// If we're dying and no more tasks should be done,
				pthread_mutex_unlock(&tp->state_lock);
				pthread_mutex_unlock(&tp->task_lock);			// just exit before dequeuing anything...
				PRINT("Thread %d unlocked the lock and returning\n",pid);
				return NULL;
				break;
		}
	}
}



