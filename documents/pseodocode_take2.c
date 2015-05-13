/**
 * Thread pool structure
 */
struct tp {
	
	// Locks for destroying the data structure.
	// Low priority lock L: for reading
	// High priority lock H: for writing (destroying)
	// Usage:
	// READ: lock(L)-->lock(H)-->{read}-->unlock(H)-->unlock(L)
	// WRITE:          lock(H)-->{write}->unlock(H)
	mutex dest_L,dest_H;
	enum state_enum {ALIVE,DO_ALL,DO_RUN} state;	// Default: ALIVE
	
	// Queue with conditional lock.
	// Threads should wait for the queue to contain something,
	// so they do a wait-lock-dequeue-unlock loop looking for
	// tasks. On the other hand, adding a task should be a 
	// signal-lock-enqueue-unlock operation.
	// When we want to destroy the thread pool, broadcast to
	// all threads with this lock so they stop waiting and try
	// to probe the queue. If the pool destruction is DO_RUN,
	// even if there are tasks in the queue the thread should
	// just exit. If the state is DO_ALL, keep looping but if
	// after locking the task queue the thread sees that there
	// are no tasks, exit - don't wait for a signal
	condition queue_not_empty_or_dying;		// The condition to signal
	mutex task_lock;						// Lock this to change the queue. Needed to allow adding a task on an empty queue
	queue tasks;							// Task queue
	
}

/**
 * Locking/unlocking access to destruction lock.
 *
 * Not very interesting, just implements this:
 *	 READ: lock(L)-->lock(H)-->{read}-->unlock(H)-->unlock(L)
 *	 WRITE:          lock(H)-->{write}->unlock(H)
 */
low_prio_dest_lock(pool) {
	lock(pool->dest_L);
	lock(pool->dest_H);
}
high_prio_dest_lock(pool) {
	lock(pool->dest_H);
}
low_prio_dest_unlock(pool) {
	unlock(pool->dest_H);
	unlock(pool->dest_L);
}
high_prio_dest_unlock(pool) {
	unlock(pool->dest_H);
}
state_enum LOW_PRIO_READ_STATE(pool) {
	state_enum state;
	low_prio_dest_lock(pool);
	state = pool->state;
	low_prio_dest_unlock(pool);
	return state;
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
add(t,pool) {
	low_prio_dest_lock(pool);				// Keep this locked while adding a task!
	if (pool->state != ALIVE) {				// If it's being destroyed, don't add a task...
		low_prio_dest_unlock(pool);
		return FAIL;
	}
	acquire_lock(pool->task_lock);			// No semaphore here: we're going to signal anyway.
											// DANGER OF DEADLOCK WITH THREAD ACQUIRING THIS LOCK?
	enqueue(t,pool->tasks);
	release_lock(pool->task_lock);			// WHICH OF THESE TWO SHOULD COME FIRST?
	signal(pool->queue_not_empty_or_dying);	// MUST WE ENFORCE IT SO THE COMPILER KNOWS?
	low_prio_dest_unlock(pool);				// Allow destruction of the thread pool
}

/**
 * Destroys the thread pool.
 *
 * 1. Lock the destruction lock (top priority... don't want to starve here)
 * 2. Change the state
 * 3. Unlock the destruction lock (now, no more tasks can be added because the state
 *	  is checked first, and the threads should know to check the state before dequeuing
 *    anything)
 * 4. Broadcast to all threads waiting for tasks: if there's nothing now, there never
 *    will be!
 * 5. MAYBE we can wait for all threads in the pool here to finish? We need to wait for
 *    them somewhere (NOT busy wait, regular wait)...
 * 6. When the threads are done, destroy all fields of the pool
 */
destroy(pool,finish_all) {
	high_prio_dest_lock(pool);
	pool->state = finish_all ? DO_ALL : DO_RUN;
	high_prio_dest_unlock(pool);
	broadcast(queue_not_empty_or_dying);	// Dying, actually. Thanks for asking.
	{WAIT FOR ALL THREADS? THEN DESTROY FIELDS OF THE THREAD POOL?}
}

/**
 * Create the thread pool.
 *
 * Pretty simple, just make sure the threads we create can't do anything
 * until we're done.
 */
tp* create(N) {
	threadPool* tp = {allocate memory and make sure it worked}
	{init variables - make sure the condition is 0, the queue is empty and state==ALIVE}
	/* DO NOT lock the task->lock! The created threads should wait for the signal anyway... */
	/* NO NEED to lock the dest_lock because no thread will check it until it's signalled anyway */
	{create N threads with the thread_func function and tp and it's argument}
	return tp;
}

/**
 * The function passed to created threads.
 *
 * 1. Lock the task queue. We're using a condition lock, so we'll
 *    give up the lock until there is a task to run OR the tp_destroy
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
 *    ... Is this OK? I asked Piazza (@281)... I hope so, because I
 *    can't think of a way to serialize task addition and destruction
 *    in a simple way...
 *    ... If this isn't allowed, how do we tell the difference between
 *    a task that we SHOULD complete even though we're in DO_RUN mode,
 *    and which task should we leave in the queue to rot?
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
 */
thread_func(pool) {
	while(1) {
		acquire_lock(pool->task_lock);	// NO BUSYWAIT! WE'LL BE STUCK HERE FOR A WHILE DURING THREADPOOL_CREATE()!
		while (is_empty(pool->queue) && LOW_PRIO_READ_STATE(pool) == ALIVE) {	// Wait for a task OR the destruction of the pool
			wait(pool->queue_not_empty_or_dying,pool->task_lock);
		}
		switch(LOW_PRIO_READ_STATE(pool)) {
			case ALIVE:								// If we're not dying, take a task and do it.
				t = dequeue(pool->queue);
				release_lock(pool->task_lock);
				DO_TASK(t);
				break;
			case DO_ALL:							// If we're dying, but we should clean up the queue:
				if (!is_empty(pool->queue)) {		// THIS TEST IS NOT USELESS! We may have got here
					t = dequeue(pool->queue);		// via a broadcast() call from tp_destroy and the
					release_lock(pool->task_lock);	// state may be DO_ALL but is_empty() may be true...
					DO_TASK(t);						// Thus, the while() loop terminated and we got here.
				}
				else {
					release_lock(pool->task_lock);
					exit(0);						// The only difference, really, between ALIVE and DO_ALL
				}
				break;
			case DO_RUN:							// If we're dying and no more tasks should be done,
				release_lock(pool->task_lock);		// just exit before dequeuing anything...
				exit(0);
				break;
		}
	}
}
