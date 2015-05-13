threadpool {
	
	list tasks;
	lock task_lock;
	condition notEmpty;
	
	int r=0;				// Destruction CREW lock
	enum dying_enum { ALIVE, FINISH_ALL, FINISH_RUNNING } dying = ALIVE;
	semaphore sRead;
	semaphore sWrite;
	
}

dying_enum read_die(threadpool* pool) {
	
	// Read the dying state (lecture 4, by the end... CREW example)
	dying_enum state;
	wait(pool->sRead);
	pool->r+=1;
	if(pool->r==1) wait(pool->sWrite);
	signal(pool->sRead);
	state = pool->dying;		// The actual read
	wait(pool->sRead);
	pool->r-=1;
	if (pool->r==0) signal(pool->sWrite);
	signal(pool->sRead);
	return state;
	
}

void thread_start(threadpool* pool) {
	while(1) {
		
		// Read the dying state (lecture 4, by the end... CREW example)
		dying_enum state = read_die(pool);
		
		// Decide if we should get a task
		if (state == FINISH_RUNNING) exit(0);
		
		// Get next task to run
		lock_acquire(pool->task_lock);
		if (state == FINISH_ALL && is_empty(pool->tasks)) exit(0);		// If there are no tasks and we're dying, die
		while(is_empty(pool->tasks)) wait(notEmpty,pool->task_lock);	// IF POOL IS BEING DESTROYED THIS IS INIFITE!
		t = dequeue_task(pool);
		lock_release(pool->task_lock);
		do_task(t);
	}
}

void destroy_pool(threadpool* pool, int should_wait) {
	dying_enum state = should_wait ? FINISH_ALL : FINISH_RUNNING;
	int we_killed = 0;
	wait(pool->sWrite);
	if (pool->dying == ALIVE) {
		pool->dying = state;
		we_killed = 1;
	}
	signal(pool->sWrite);
	if (we_killed) {
		... destroy...	// DANGER: If there were never any tasks, there are now threads waiting
						// for a task. THEY WILL NEVER EXIT IF THEY ARE NOT GIVEN TASKS!
		MAYBE: If the task list is empty and there are still threads, add dummy task
		OR, BETTER: Send Killall to the threads? WON'T WORK: WE NEED TO LET THEM RUN...
	}
}

threadpool* create(int N) {
	threadpool* tp = new threadpool; // malloc etc...
	if (!tp) FAIL();
	tp->r = 0;
	tp->dying = ALIVE;
	createlist(tp->tasks);
	int i;
	lock_acquire(tp->task_lock);
	for (i=0; i<N; ++i) {
		create thread with function destroy_pool and param tp
	}
	lock_release(tp->task_lock);
}


