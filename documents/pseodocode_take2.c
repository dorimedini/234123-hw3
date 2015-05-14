/**
 * Thread pool structure
 */
struct tp {
	
	/**
	 * QUESTION:
	 *
	 * How should we implement R/W protection of the "state" field?
	 * This is a readers-writers problem. We shouldn't allow multiple
	 * writers ("destroy" can only be done once), we should allow lots
	 * of readers (all threads read this all the time) and we should
	 * let the writer get high priority.
	 *
	 * Remember - we NEED to allow infinite simultaneous readers of the
	 * pool state, because of this possible scenario:
	 * - add() is called, we call start_read()
	 * - CONTEXT SWITCH
	 * - A thread start it's while() loop and does acquire_lock(pool->task_lock) 
	 * - Enters the inner while() loop, calls read_state()
	 * If we use mutex locks, read_state() needs to wait for add(), and add()
	 * needs to wait for the thread!
	 *
	 * Useful links:
	 * - http://lass.cs.umass.edu/~shenoy/courses/fall08/lectures/Lec11.pdf		(PAGES 4-5)
	 * - http://en.wikipedia.org/wiki/Readers%E2%80%93writers_problem#Second_readers-writers_problem
	 */
	int r_num, w_num; 						// Number of current readers and number of writers entering
	semaphore r_num_mutex, w_num_mutex;		// Lock these when updating r_num or w_num
	semaphore r_entry;						// The first lock locked when a reader starts trying to read
	semaphore read_try;						// The second lock locked when a reader starts trying to read,
											// also used by writers to block readers arriving after the writer
											// wants to write
	semaphore state_lock;					// Mutex lock for the state field itself
	state_num state // Can be ALIVE, DO_ALL, DO_RUN
	
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
 * class, but with another (read_try) lock before entering the reader
 * section. It's job is to be locked by writers as well, so writers can
 * signal to readers not to request the resource.
 *
 * For full explanation as to how this works, see:
 * http://en.wikipedia.org/wiki/Readers%E2%80%93writers_problem#Second_readers-writers_problem
 */
start_read(pool) {
	r_entry.P();		// Start reader section
	read_try.P();		// Looks useless, but notice that this isn't used in the exit section.
						// This is because this is also locked by writers trying to write,
						// to block readers that come later from getting the resource before
						// them.
	r_num_mutex.P();	// For editing r_num
	r_num++;
	if (r_num == 1)		// If this is the first reader, lock the data
		state_lock.P();
	r_num_mutex.V();	// Stop editing r_num
	read_try.V();		// We've successfully entered the reading area
	r_entry.V();		// Done with the pre-read phase
}
end_read(pool) {
	r_num_mutex.P();	// For editing r_num
	r_num--;
	if (r_num == 0)		// If we're the last reader to finish, free the resource
		state_lock.V();
	r_num_mutex.V();	// Stop editing r_num
}
start_write(pool) {
	w_num_mutex.P();	// Start editing w_num
	w_num++;
	if (w_num == 1)		// If we're the first one trying to write, stop subsequent readers from
		read_try.P();	// successfully passing the "start_read" phase
	w_num_mutex.V();	// Stop editing w_num
	state_lock.P();		// Lock the data field! We're going to write to it
}
end_write(pool) {
	state_lock.V();//release file
	w_num_mutex.P();//reserve exit section
	w_num--;//indicate you're leaving
	if (w_num == 0)//checks if you're the last writer
		read_try.V();//if you're last writer, you must unlock the readers. Allows them to try enter CS for reading
	w_num_mutex.V();//release exit section
}

/**
 * Use the above to quickly read and return the state of the thread pool
 */
state_enum read_state(pool) {
	start_read(pool);
	state_enum state = pool->state;
	end_read(pool);
	return state;
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
	start_write(pool);
	if (pool->state != ALIVE) {					// Destruction already in progress.
		end_write(pool);						// This can happen if destroy() is called twice fast
		return;
	}
	pool->state = finish_all ? DO_ALL : DO_RUN;	// Enter destroy mode
	end_write(pool);							// Allow reading the state
	broadcast(queue_not_empty_or_dying);		// Dying, actually. Thanks for asking. Tell everyone!
	{WAIT FOR ALL THREADS? THEN DESTROY FIELDS OF THE THREAD POOL?}
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
	start_read(pool);						// Keep this locked while adding a task!
	if (pool->state != ALIVE) {				// If it's being destroyed, don't add a task...
		end_read(pool);
		return FAIL;
	}
	acquire_lock(pool->task_lock);			// No semaphore here: we're going to signal anyway.
											// DANGER OF DEADLOCK WITH THREAD ACQUIRING THIS LOCK!
	enqueue(t,pool->tasks);
	release_lock(pool->task_lock);			// WHICH OF THESE THREE SHOULD COME FIRST?
	end_read(pool);							// Allow destruction of the thread pool
	signal(pool->queue_not_empty_or_dying);	// MUST WE ENFORCE IT SO THE COMPILER KNOWS?
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
	state_enum state;
	while(1) {
		acquire_lock(pool->task_lock);	// This is OK because during INIT, we don't lock the task queue (after its creation)
		while (is_empty(pool->queue) && (state = read_state(pool)) == ALIVE)	// Wait for a task OR the destruction of the pool
			wait(pool->queue_not_empty_or_dying,pool->task_lock);				// Either one gives a signal
		switch(state) {
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
				else {								// If we're here, there are no more tasks to dequeue!
					release_lock(pool->task_lock);	// As we're being destroyed anyway, exit.
					exit(0);
				}
				break;
			case DO_RUN:							// If we're dying and no more tasks should be done,
				release_lock(pool->task_lock);		// just exit before dequeuing anything...
				exit(0);
				break;
		}
	}
}
