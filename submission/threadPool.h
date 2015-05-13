#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <pthread.h>
#include "osqueue.h"

/**
 * The three states a thread pool can be in:
 * ALIVE:  Normal.
 * DO_ALL: In the process of being destroyed, but should complete all existing tasks
 * DO_RUN: In the process of being destroyed, without completing any enqueued tasks
 */
typedef enum state_enum {
	ALIVE,
	DO_ALL,
	DO_RUN
};

typedef struct thread_pool
{
	
	// Total number of threads
	int N;
	
	// TODO: Everything else :)
	
	
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
