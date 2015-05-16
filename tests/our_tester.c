/**
 * HW3 Tester
 *
 * WHAT YOU SHOULD SEE:
 * A table of threads doing tasks. For example:
 * 			================================
 * 			Task execution table by threads:
 * 			================================
 * 			  | T 1 (TID=20141) | T 2 (TID=20144) | T 3 (TID=20143) | T 4 (TID=20142) 
 * 			--------------------------------------------------------------------------
 * 			  |      START      |                 |                 |                 |	  (The first thread created started it's task)
 * 			  |                 |      START      |                 |                 |	  (The second thread started it's task)
 * 			  |                 |                 |      START      |                 |	  (The third thread started it's task)
 * 			  |                 |                 |                 |      START      |	  (The fourth thread started it's task)
 * 			  |       END       |                 |                 |                 |   (The first thread finished it's task)
 * 			  |      START      |                 |                 |                 |   (The first thread started another task)
 * 			  |                 |                 |       END       |                 |   (The third thread finished it's task)
 * 			  |                 |                 |      START      |                 |   (....etc)
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |       END       |                 |
 * 			  |                 |                 |      START      |                 |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |       END       |                 |                 |                 |
 * 			  |      START      |                 |                 |                 |
 * 			  |       END       |                 |                 |                 |
 * 			  |      START      |                 |                 |                 |
 * 			  |                 |       END       |                 |                 |
 * 			  |                 |      START      |                 |                 |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |                 |                 |       END       |
 * 			  |                 |                 |                 |      START      |
 * 			  |                 |       END       |                 |                 |
 * 			  |       END       |                 |                 |                 |
 * 			  |                 |                 |       END       |                 |
 * 			  |                 |                 |                 |       END       |
 * 			================================
 * 			 Done printing execution table! 
 * 			================================
 * The output may not help very much, but it may give you some clues.
 * It may be difficult to read in a narrow screen, so try calling ./our_tester>log instead of calling
 * ./our_tester, so you can view the log like a human being.
 * We suggest you add the entire #ifndef HW3_DEBUG block to threadPool.h and use the PRINT() macro
 * in threadPool.c to help you figure out what's going on.
 *
 * IF YOU DO: Remember to set USE_GETTID and HW3_DEBUG to 0 before submitting!
 *
 * USAGE NOTES:
 * @ If you're running this on VMWare (Linux 2.4XXX), set USE_GETTID to 0 (defined as a macro bellow).
 *   Otherwise, you'll want to set USE_GETTID to 1 to use gettid() instead of getpid() - later versions
 *   of Linux return the same value of getpid() for threads in the same process, so calling getpid()
 *   would be useless.
 *   To get a thread ID, use TID(). It will choose between gettid() and getpid() for you.
 * @ If you want to print extra stuff in the tests, set HW3_DEBUG to 1 (also defined bellow)
 *   and use PRINT() instead of printf() (so you can easily get rid of extra garbage printing).
 * @ To write your own tests, you may find these useful:
 *   - INIT(n)
 *   - DESTROY(should)
 *   - SETUP_PTRS(n)
 *   - CREATE_TASKS(n,tp)
 *   - CREATE_TASKS_DELAY(n,tp,d)
 *   - ASSERT_TASKS_DONE(n)
 *   Read about them next to their definition. For some examples, see the existing test functions called
 *   via main().
 * @ We've set up a default task for threads to do. By default (if the global random_flag is set to 0) it
 *   counts to some high random number (so different threads take different lengths of time to complete
 *   their task), and you can set random_flag=X to force all threads to count to X.
 *   Also, whenever you use the macros to creates the tasks, an array called completion[] will be created.
 *   completion[i]=0 <==> Task #i has been completed. Thus, you can make sure tasks are completed by testing
 *   the value of elements of completion[] (this is what ASSERT_TASKS_DONE does)
 */


#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "threadPool.h"

/**
 * Debug printing method.
 *
 * Uses the HW3_DEBUG flag to decide whether or not to print.
 *
 * We use an ifndef directive here because these macros are defined
 * in (our) threadPool.h, so for portability we redefine them here.
 */
#ifndef HW3_DEBUG

#define HW3_DEBUG 0	// Set to 1 to print, set to 0 to prevent printing
#define USE_GETTID 1
#define PRINT(...) do { \
		if (HW3_DEBUG) printf(__VA_ARGS__); \
	} while(0)

#define PRINT_IF(cond,...) do { \
		if (cond) PRINT(__VA_ARGS__); \
	} while(0)

#if USE_GETTID
#include <sys/syscall.h>		// For gettid()
#define TID() syscall(SYS_gettid)
#else
#define TID() getpid()			// If syscalls.h isn't included, syscall(SYS_gettid) won't appear anywhere in the code
#endif

#endif

/**
 * Some simple testing macros
 */
#define FAIL(msg) do { \
		printf("FAIL! In %s, Line %d: " #msg "\n",__FILE__,__LINE__); \
		return 0; \
	} while(0)

#define ASSERT(x) do { \
		if (!(x)) FAIL(#x " is false"); \
	} while(0)

#define RUN_TEST(b) do { \
		printf("Running " #b "...\n"); \
		if (b()) printf("OK\n"); \
	} while(0)


/**
 * Thread tasks
 *
 * These functions / global variables are used to give the threads something to do.
 * They're used for giving them tasks of varying lengths and printing them.
 */
// Use these global variables to print the output of a test in a nice way
int* thread_ids;
int total_threads;
OSQueue* task_log;

// Use this global variable to flag the random_task to be less random...
int random_flag;

// A generic task to give to threads.
// Expects a valid pointer to an integer (so we can
// check to see if it's done)
// Assumes the integer sent is 0. Counts to some number, and when
// it's done inserts "1" to the int pointer sent.
void task_log_start();	// Declare these in advance, random_task uses them
void task_log_end();
void random_task(void* x) {
	int i,r=(rand()%100)*50000;	// 50000~5000000. I hope 5000000 takes a while to count to...
	if (random_flag)
		r = random_flag;		// If we don't want it to be random, flag it...
	task_log_start();
	for (i=0; i<r; ++i);		// Count (should take a while)
	*((int*)x)=1;				// Done
	task_log_end();
	return;
}


// Use these functions in random_task to print the task state nicely.
// First, makes sure the thread exists in thread_ids (in log_start only).
// If not, adds it.
// Next, adds a new string to print in task_log* (both start() and end()).
typedef struct task_log_t {
	int index;
	int started;
} TaskLog;
void task_log_start() {
	
	// Find the thread index.
	// If it doesn't exists, create it
	int index, tid = TID();
	for (index=0; index<total_threads; ++index) {
		if (!thread_ids[index]) {
			thread_ids[index] = tid;
			break;
		}
		else if(thread_ids[index] == tid)
			break;
	}
	
	// Log the beginning of the task
	TaskLog* t = (TaskLog*)malloc(sizeof(TaskLog));
	t->index = index;
	t->started = 1;
	osEnqueue(task_log, (void*)t);
	
}
void task_log_end() {
	
	// Find the thread index.
	int index=-1, tid = TID();
	while(thread_ids[++index] != tid && index<total_threads);
	
	// Log the beginning of the task
	TaskLog* t = (TaskLog*)malloc(sizeof(TaskLog));
	t->index = index;
	t->started = 0;
	osEnqueue(task_log, (void*)t);
	
}
void destroy_log() {
	while(!osIsQueueEmpty(task_log)) {
		free((TaskLog*)osDequeue(task_log));
	}
	osDestroyQueue(task_log);
}
void print_task_table() {
	int i,j;
	printf("================================\n");
	printf("Task execution table by threads:\n");
	printf("================================\n");
	printf("  ");
	for (i=0; i<total_threads; ++i)
		printf("| T%2d (TID=%5d) ",i+1,thread_ids[i]);
	printf("\n--");
	for (i=0; i<total_threads; ++i)
		printf("------------------");
	printf("\n");
	while(!osIsQueueEmpty(task_log)) {
		TaskLog* t = (TaskLog*)osDequeue(task_log);
		printf("  |");
		for (i=0; i<t->index; ++i)
			printf("                 |");
		printf("      %s      |",t->started ? "START" : " END ");
		for (i=t->index+1; i<total_threads; ++i)
			printf("                 |");
		printf("\n");
		free(t);
	}
	printf("================================\n");
	printf(" Done printing execution table! \n");
	printf("================================\n");
}

/**
 * Macros to be used in test functions.
 *
 * For some examples, see the tests bellow.
 */
// Creates a thread pool with n threads, and updates the globals.
#define INIT(n) \
	total_threads = n; \
	task_log = osCreateQueue(); \
	thread_ids = (int*)malloc(sizeof(int)*n); \
	do { \
		int i; \
		for(i=0; i<n; ++i) \
			thread_ids[i]=0; \
	} while(0); \
	ThreadPool* tp = tpCreate(n)
// Destroys the thread pool (with the should_wait parameter)
// and takes care of other cleanup
#define DESTROY(should) do { \
		tpDestroy(tp,should); \
		print_task_table(); \
		free(thread_ids); \
		total_threads = 0; \
		destroy_log(); \
	} while(0)
// Sets up an array of integers, so its pointers
// can be sent to the random task.
#define SETUP_PTRS(n) \
	int completion[n]; \
	do { \
		int i; \
		for (i=0; i<n; ++i) \
			completion[i]=0; \
	} while(0)
// Create n random tasks and insert into the pool
#define CREATE_TASKS(n,tp) CREATE_TASKS_DELAY(n,tp,0)
// Create n random tasks, insert them into the pool with some
// delay between insertions
#define CREATE_TASKS_DELAY(n,tp,d) \
		SETUP_PTRS(n); \
		do { \
			int i,j; \
			for (i=0; i<n; ++i) \
				ASSERT(!tpInsertTask(tp,random_task,(void*)(completion+i))); \
			for (j=0; j<d; ++j); \
		} while(0)
// After all tasks should be done, use this to make sure it's true
#define ASSERT_TASKS_DONE(n) do { \
		int i; \
		for (i=0; i<n; ++i) ASSERT(completion[i]); \
	} while(0)

	
/**********************************************************************************************
 **********************************************************************************************
                                   TEST FUNCTIONS AND MAIN()
 **********************************************************************************************
 *********************************************************************************************/

/**
 * Give the threads long jobs
 */
int long_test() {
	random_flag=10000000;
	INIT(10);
	CREATE_TASKS(100,tp);
	DESTROY(1);
	ASSERT_TASKS_DONE(100);
	return 1;
}

/**
 * Just give them lots of tasks at once
 */
int stress_test() {
	random_flag = 0;
	INIT(10);
	CREATE_TASKS(100,tp);
	DESTROY(1);
	ASSERT_TASKS_DONE(100);
	return 1;
}

/**
 * Give a task every X time, to give the threads some time to wait
 */
int delay_test() {
	random_flag = 0;
	INIT(10);
	CREATE_TASKS_DELAY(100,tp,1000000);
	DESTROY(1);
	ASSERT_TASKS_DONE(100);
	return 1;
}

int main() {
	
	// Initialize the random number generator
	srand(time(NULL));
	
	// Run tests
	RUN_TEST(long_test);
	RUN_TEST(stress_test);
	RUN_TEST(delay_test);
	
	return 0;
}


