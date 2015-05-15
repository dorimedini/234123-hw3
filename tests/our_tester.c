#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "threadPool.h"


/**
 * Some simple testing macros
 */
#define FAIL(msg) do { \
		printf("FAIL! In %s, Line %d: " #msg "\n",__FILE__,__LINE__); \
		return false; \
	} while(0)

#define ASSERT(x) do { \
		if (!(x)) FAIL(#x " is false"); \
	} while(0)

#define RUN_TEST(b) do { \
		printf("Running " #b "..."); \
		if (b()) printf("OK\n"); \
	} while(0)


/**
 * A generic task to give to threads.
 *
 * Expects a valid pointer to an integer (so we can
 * check to see if it's done)
 *
 * Assumes the integer sent is 0. Counts to some number, and when
 * it's done inserts "1" to the int pointer sent.
 */
void random_task(void* x) {
	int i,r=(rand()%100)*500;	// 500~50000. I hope 50000 takes a while to count to...
	for (i=0; i<r; ++i);		// Count (should take a while)
	*((int*)x)=1;				// Done
	return;
}

/**
 * Sets up an array of integers, so its pointers
 * can be sent to the random task.
 */
#define SETUP_PTRS(n) \
	int completion[n]; \
	do { \
		int i; \
		for (i=0; i<n; ++i) \
			completion[i]=0; \
	} while(0)


/**
 * Create n random tasks and insert into the pool
 */
#define CREATE_TASKS(n,tp) \
		SETUP_PTRS(n); \
		do { \
			int i; \
			for (i=0; i<n; ++i) \
				ASSERT(!tpInsertTask(tp,random_task,(void*)(completion+i))); \
		} while(0)

/**
 * After all tasks should be done, use this to make sure it's true
 */
#define ASSERT_TASKS_DONE(n) do { \
		int i; \
		for (i=0; i<n; ++i) ASSERT(completion[i]); \
	} while(0)
	

int main() {
	
	// Init the random number generator
	srand(time(NULL));
	
	return 0;
}