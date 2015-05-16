#include <stdio.h>
#include <stdlib.h>
#include "osqueue.h"
#include "threadPool.h"


void hello (void* a) {
	PRINT("Thread %d printing\n",TID());
	printf("hello\n");
}


void test_thread_pool_sanity() {
	int i;

	PRINT("Create:\n");
	ThreadPool* tp = tpCreate(5);

	PRINT("Start making tasks.\n");
	for(i=0; i<5; ++i) {
		PRINT("Task #%d:\n",i+1);
		tpInsertTask(tp,hello,NULL);
	}

	PRINT("Inserted all tasks. Destroying threadpool with DO_ALL:\n");
	tpDestroy(tp,1);
	PRINT("Done destroying\n");
}


int main() {
	PRINT("Start threadpool sanity test:\n");
	test_thread_pool_sanity();
	return 0;
}
