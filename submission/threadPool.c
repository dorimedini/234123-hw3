
#include "threadPool.h"

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
 * CREW implementation for the thread pool state.
 *
 * Implementation and documentation bellow.
 */
void start_read(ThreadPool*);
void end_read(ThreadPool*);
int start_write(ThreadPool*);
void end_write(ThreadPool*);

/**
 * The function sent to all threads in the thread pool
 */
void* thread_func(void*);

/**
 * Reads and returns the current state of the thread pool.
 *
 * Uses the reader functions above (implemented bellow).
 */
State read_state(ThreadPool* tp) {
	start_read(tp);
	State state = tp->state;
	end_read(tp);
	return state;
}




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
	
	// Locks and things
	pthread_mutex_init(&tp->task_lock, NULL);
	sem_init(&tp->r_num_mutex, 0, 1);
	sem_init(&tp->w_flag_mutex, 0, 1);
	sem_init(&tp->r_entry, 0, 1);
	sem_init(&tp->r_try, 0, 1);
	sem_init(&tp->state_lock, 0, 1);
	pthread_cond_init(&tp->queue_not_empty_or_dying, NULL);
	
	// Basic fields
	tp->N = num;
	tp->r_num = 0;
	tp->w_flag = 0;
	tp->state = ALIVE;
	tp->tasks = osCreateQueue();
	
	// Create threads!
	// No need to lock anything, the threads are now allowed
	// to read the task queue and the state anyway - neither
	// of them are going to be touched here.
	int i,ret;
	for (i=0; i<num; ++i) {
		ret = pthread_create(tp->threads + i, NULL, thread_func, (void*)tp);
		PRINT("Creating thread #%d, return value: %d\n",i+1,ret);
	}
	
	// Return the ThreadPool
	return tp;
	
}




/**
 * Destroys the thread pool.
 *
 * 1. Lock the destruction lock (top priority... don't want to starve here)
 * 2. Change the state
 * 3. Unlock the destruction lock (now, no more tasks can be added because the state
 *	  is checked first, and the threads should know to check the state before dequeuing
 *    anything)
 * 4. Lock the tasks lock, so we're sure all threads are waiting for a signal (or for
 *    the lock) to prevent deadlock (see comments bellow).
 * 5. Broadcast to all threads waiting for tasks: if there's nothing now, there never
 *    will be!
 * 6. Unlock the tasks lock (to allow the threads to do their thing)
 * 7. Wait for all threads to finish
 * 8. Destroy all fields of the pool
 */
void tpDestroy(ThreadPool* tp, int should_wait_for_tasks) {
	
	if (!tp)												// Sanity check
		return;
	
	if (!start_write(tp)) {									// Someone is already writing to pool->state!
		return;												// This should only happen ONCE...
	}
	if (tp->state != ALIVE) {								// Destruction already in progress.
		end_write(tp);										// This can happen if destroy() is called twice fast
		return;
	}
	tp->state = should_wait_for_tasks ? DO_ALL : DO_RUN;	// Enter destroy mode
	end_write(tp);											// Allow reading the state

	// Make sure the queue isn't busy.
	// We shouldn't encounter deadlock/livelock here because threads wait for signals, and the only
	// possible source of a signal is here or in tpInsertTask().
	// If we lock this only AFTER we change the/ destruction state, we can create a situation where
	// a thread waits forever for a signal that will never come:
	// - A thread is in it's while loop, the queue is
	//   empty and the pool isn't being destroyed
	//   so it enters the body of the loop. Before it
	//   starts waiting for a signal...
	// - CS-->Main thread
	// - Main thread calls destroy, doesn't wait for
	//   the task_lock and writes the new destruction
	//   state. Then, broadcasts a signal to listening
	//   threads.
	// - CS-->Previous thread
	// - Starts waiting for a signal that will never
	//   come
	// By locking here before broadcasting the destruction, we make sure all threads are waiting for
	// the lock or for the signal! Either way, after the signal, the threads will know what to do and
	// exit the loop.
	pthread_mutex_lock(&tp->task_lock);
	pthread_cond_broadcast(&tp->queue_not_empty_or_dying);
	pthread_mutex_unlock(&tp->task_lock);
	
	// Wait for all threads to exit (this could take a while)
	int i;
	for (i=0; i<tp->N; ++i) {
		pthread_join(tp->threads[i], NULL);
	}
	
	// Cleanup!
	// Tasks (we can still lock here):
	pthread_mutex_lock(&tp->task_lock);
	while (!osIsQueueEmpty(tp->tasks)) {
		Task* t = (Task*)osDequeue(tp->tasks);
		free(t);
	}
	osDestroyQueue(tp->tasks);
	pthread_mutex_unlock(&tp->task_lock);
	
	// Locks:
	pthread_mutex_destroy(&tp->task_lock);
	sem_destroy(&tp->r_num_mutex);
	sem_destroy(&tp->w_flag_mutex);
	sem_destroy(&tp->r_entry);
	sem_destroy(&tp->r_try);
	sem_destroy(&tp->state_lock);
	pthread_cond_destroy(&tp->queue_not_empty_or_dying);
	
	// Last cleanup, and out:
	free(tp->threads);
	free(tp);
	return;
}




/**
 * Add task.
 *
 * Basically:
 * 1. Make sure we're not being destroyed, and prevent destruction until we're done (dest_lock)
 * 2. Lock the task queue (unconditionally - no upper limit on number of tasks)
 * 3. Add a task
 * 4. Release the task queue lock and signal that the queue isn't empty (which one first?)
 * 5. Unlock the destruction lock (now we can kill the thread pool)
 */
int tpInsertTask(ThreadPool* tp, void (*func)(void *), void* param) {
	
	start_read(tp);				// Read the state, and generally prevent it's change
	if (tp->state != ALIVE) {	// If the thread pool is dying,
		end_read(tp);			// release the state for reading (no more writing will be done)
		return -1;				// and return indication of destruction.
	}
	
	// Try creating the task now, before locking the queue but after reading, because this
	// takes a long time so we don't want to prevent tasks from running. We're currently still
	// "reading" the thread pool state, but that's OK because a. it's a CREW lock so the threads
	// can read and see that the thread pool is a live and b. if a writer wants to write to the
	// state it's because the writer wants to destroy the thread pool - we want to prevent that
	// anyway at this point.
	Task* t = (Task*)malloc(sizeof(Task));
	if (!t) {
		end_read(tp);
		return -1;
	}
	t->func = func;
	t->param = param;
	
	// Editing the task queue now.
	// There's no danger of deadlock with thread acquiring this lock: If a thread isn't waiting
	// for a signal then all it does is read the state (we can allow that here even if we're
	// waiting for the task_lock) or do it's thing (dequeue or exit). Either way, it'll let go
	// of the lock eventually. We don't need to give the "add" function priority because the
	// worst case scenario is that it'll take some time to enqueue the task... But that's only
	// if the threads are busy, so the task won't get done anyway.
	pthread_mutex_lock(&tp->task_lock);
	osEnqueue(tp->tasks,(void*)t);
	
	// Signal before releasing the lock - make a thread wait for the lock.
	pthread_cond_signal(&tp->queue_not_empty_or_dying);	
	pthread_mutex_unlock(&tp->task_lock);
	
	// Allow destruction of the thread pool (allow writing to pool->state)
	end_read(tp);
	
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
 *    up.
 * 2. Wait for the signal (task inserted, or tp being destroyed).
 * 3. Now that the queue is locked, check the destruction state. This
 *    state should be valid because a. the change from ALIVE to
 *    something else is a one-way change, b. even if the following
 *    happened:
 *    - Task added
 *    - Thread got out of the WHILE loop
 *    - CONTEXT SWITCH
 *    - Main thread (pool creator) called tp_destroy, state changed
 *    - CONTEXT SWITCH
 *    - Back to our thread, got to the switch() statement and found
 *      out we're dying
 *    This is the desired behaviour (Piazza @281) - we do not need to
 *    make sure tasks added before calls to tpDestroy will  be executed
 *    if tpDestroy is called in DO_RUN mode, even if all threads were
 *    available when the task was added.
 * 4. If we're ALIVE, that means pool->queue IS NOT EMPTY (otherwise we
 *    would still be in the while loop, because you can't change DO_RUN
 *    or DO_ALL back to ALIVE so there's no danger we left the while()
 *    loop because of state!=ALIVE but got to state==ALIVE in the
 *    switch), so we can just dequeue a task and run it (remember to
 *    unlock before running!).
 * 5. If we're DO_ALL, it's like ALIVE but first check if there's
 *    something to run (unlike the ALIVE state, we don't know for sure).
 *    If there is, run it; otherwise, exit (no more tasks will come).
 * 6. If we're DO_RUN, exit. Don't take another task, leave them to rot.
 * 7. Rinse and repeat
 */
void* thread_func(void* void_tp) {
	
	int pid = getpid();
	PRINT("Thread %d started it's function\n",pid);
	
	// Some useful variables
	State state;
	Task* t;
	ThreadPool* tp = (ThreadPool*)void_tp;

	// Main thread task
	while(1) {
		
		// Get the task lock, when we need it (task to do or we're dying)
		pthread_mutex_lock(&tp->task_lock);										// This is OK because during INIT, we don't lock the task queue (after its creation)
		while (osIsQueueEmpty(&tp->tasks) && (state = read_state(tp)) == ALIVE)	// Wait for a task OR the destruction of the pool
			pthread_cond_wait(&tp->queue_not_empty_or_dying,&tp->task_lock);	// Either one gives a signal
		
		switch(state) {
			case ALIVE:										// If we're not dying, take a task and do it.
				t = (Task*)osDequeue(tp->tasks);
				pthread_mutex_unlock(&tp->task_lock);
				PRINT("Thread %d doing it's task\n",pid);
				t->func(t->param);
				free(t);
				break;
			case DO_ALL:									// If we're dying, but we should clean up the queue:
				if (!osIsQueueEmpty(tp->tasks)) {			// THIS TEST IS NOT USELESS! We may have got here
					t = (Task*)osDequeue(tp->tasks);		// via a broadcast() call from tp_destroy and the
					pthread_mutex_unlock(&tp->task_lock);	// state may be DO_ALL but is_empty() may be true...
					PRINT("Thread %d doing it's task\n",pid);
					t->func(t->param);						// Thus, the while() loop terminated and we got here.
					free(t);
				}
				else {										// If we're here, there are no more tasks to dequeue!
					pthread_mutex_unlock(&tp->task_lock);	// As we're being destroyed anyway, exit.
					return NULL;
				}
				break;
			case DO_RUN:									// If we're dying and no more tasks should be done,
				pthread_mutex_unlock(&tp->task_lock);		// just exit before dequeuing anything...
				return NULL;
				break;
		}
	}
}




/**
 * Readers-writers lock mechanism, with preference to writers.
 *
 * This allows multiple readers at the same time (which is important,
 * for reasons described in the thread pool struct). Unlike the lecture
 * slides version, this one gives writers higher priority than readers.
 * This prevents writer starvation.
 * In theory, readers can starve, but the "write" operation used here
 * should only be done once anyway - writing with these functions is
 * how we implement the destruction of the thread pool. If many writers
 * are starving the readers, that's the users fault. In addition, the
 * write section (critical section) is VERY short (simple variable update)
 * so starving readers would be hard...
 *
 * In a nutshell, this is like the readers-writers mechanism seen in
 * class, but with another (r_try) lock before entering the reader
 * section. It's job is to be locked by writers as well, so writers can
 * signal to readers not to request the resource.
 *
 * For full explanation as to how this works, see the link bellow (we did
 * something a little bit different... we don't want multiple writers anyway
 * so there's no need for a writer counter, just a flag will do):
 *
 * http://en.wikipedia.org/wiki/Readers%E2%80%93writers_problem#Second_readers-writers_problem
 */
// Enter read block
void start_read(ThreadPool* tp) {
	sem_wait(&tp->r_entry);		// Start reader section
	sem_wait(&tp->r_try);		// Looks useless, but notice that this isn't used in the exit section.
								// This is because this is also locked by writers trying to write,
								// to block readers that come later from getting the resource before
								// them.
	sem_wait(&tp->r_num_mutex);	// For editing r_num
	tp->r_num++;
	if (tp->r_num == 1)			// If this is the first reader, lock the data
		sem_wait(&tp->state_lock);
	sem_post(&tp->r_num_mutex);	// Stop editing r_num
	sem_post(&tp->r_try);		// We've successfully entered the reading area
	sem_post(&tp->r_entry);		// Done with the pre-read phase
}


// Exit read block
void end_read(ThreadPool* tp) {
	sem_wait(&tp->r_num_mutex);	// For editing r_num
	tp->r_num--;
	if (tp->r_num == 0)			// If we're the last reader to finish, free the resource
		sem_post(&tp->state_lock);
	sem_post(&tp->r_num_mutex);	// Stop editing r_num
}


// Tries to enter write block, fails if someone is writing already.
// If fails, returns 0. Otherwise, returns 1.
int start_write(ThreadPool* tp) {
	sem_wait(&tp->w_flag_mutex);	// Start editing w_flag
	if (tp->w_flag) {				// If someone is already writing, we should do nothing because the
		sem_post(&tp->w_flag_mutex);// "state" field should only be edited ONCE anyway...
		return 0;					// Say we failed to start writing
	}
	tp->w_flag = 1;					// This process is the only one trying to write (verified by the w_flag
	sem_wait(&tp->r_try);			// test above) so lock readers out.
	sem_post(&tp->w_flag_mutex);	// Stop editing w_flag
	sem_wait(&tp->state_lock);		// Lock the data field! We're going to write to it
	return 1;						// Succeeded to start writing
}


// Exit write block
void end_write(ThreadPool* tp) {
	sem_post(&tp->state_lock);		// Release the data field
	sem_wait(&tp->w_flag_mutex);	// Start editing w_flag
	tp->w_flag = 0;					// This was the ONLY thread that was trying to write (verified in the
	sem_post(&tp->r_try);			// start_write() function), so allow readers in now.
	sem_post(&tp->w_flag_mutex);	// Stop editing w_flag
}

