#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include "osqueue.h"

/**
 * Some debugging macros (conditional printing)
 */
#define HW3_DEBUG 0
#define USE_GETTID 0

#if HW3_DEBUG
	#define PRINT(...) printf(__VA_ARGS__)
	#define PRINT_IF(cond,...) if(cond) PRINT(__VA_ARGS__)
#else
	#define PRINT(...)		// If we're not debugging, just erase these lines
	#define PRINT_IF(...)
#endif
	
/**
 * Include syscalls.h and redefine how we get the thread ID,
 * depending on the DEBUG mode of the code.
 *
 * Used to print thread IDs. Maybe we're not allowed to submit
 * with it (because in the version of Linux we're supposed to
 * support, getpid() returns the thread ID so we won't need
 * gettid()...), so enclose it in a preprocessor directive
 */
#if USE_GETTID
	#include <sys/syscall.h>		// For gettid()
	#define TID() syscall(SYS_gettid)
#else
	#define TID() getpid()			// If syscalls.h isn't included, syscall(SYS_gettid) won't appear anywhere in the code
#endif


/**
 * State definitions.
 *
 * These are the states a thread pool can be in.
 */
typedef enum state_t {
	ALIVE,	// Thread pool is running
	DO_ALL,	// Thread pool is being destroyed, will finish all tasks in the queue first
	DO_RUN	// Thread pool is being destroyed, only finish currently running tasks
} State;

typedef struct thread_pool {
	
	int N;				// Total number of threads
#if HW3_DEBUG
	int* tids;			// Array of thread IDs (used for debugging)
#endif
	pthread_t* threads;	// Array of pthread identifiers. Used to wait for
						// all threads during destruction of this struct.
	
	/**
	 * These fields are used to implement CREW on the state field.
	 * The implementation favours the writer (only one writer should
	 * EVER write to the state because that's how we implement the
	 * thread pool' destruction) by allowing the writer to block
	 * entry into the "start reading" section for readers.
	 */
	int r_num, w_flag; 					// INIT: 0. Number of current readers and an on/off flag for writing
	sem_t r_num_mutex, w_flag_mutex;	// INIT: 1. Lock these when updating r_num or w_flag
	sem_t r_entry;						// INIT: 1. The first lock locked when a reader starts trying to read
	sem_t r_try;						// INIT: 1. The second lock locked when a reader starts trying to read,
										// also used by writers to block readers arriving after the writer
										// wants to write
	sem_t state_lock;					// INIT: 1. 'Mutex' lock for the state field itself
	State state; 						// Can be ALIVE, DO_ALL, or DO_RUN.
	
	/**
 	 * Queue with condition lock.
	 *
	 * Threads should wait for the queue to contain something,
	 * so they do a wait-lock-dequeue-unlock loop looking for
	 * tasks. On the other hand, adding a task should be a 
	 * signal-lock-enqueue-unlock operation.
	 *
	 * When we want to destroy the thread pool, broadcast to
	 * all threads with this lock so they stop waiting and try
	 * to probe the queue. If the pool destruction is DO_RUN,
	 * even if there are tasks in the queue the thread should
	 * just exit. If the state is DO_ALL, keep looping but if
	 * after locking the task queue the thread sees that there
	 * are no tasks, exit - don't wait for a signal
	 */
	pthread_cond_t queue_not_empty_or_dying;// The condition to signal
	pthread_mutex_t task_lock;				// Lock this to change the queue. Needed to allow adding a task on an empty queue
	pthread_mutexattr_t mutex_type;			// Use this to specify the ERRORCHECK type of mutex
	OSQueue* tasks;							// Tasks queue
	
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
