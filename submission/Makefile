all: test.c libthreadPool.a osqueue.h threadPool.h
	gcc -L. test.c -lthreadPool -lpthread -o a.out

tester: our_tester.c libthreadPool.a osqueue.h threadPool.h
	gcc -L. our_tester.c -lthreadPool -lpthread -o our_tester

osqueue.o: osqueue.c
	gcc -c -L. osqueue.c -lpthread

threadPool.o: osqueue.o threadPool.c osqueue.h
	gcc -c -L. threadPool.c -lpthread
	
libthreadPool.a: threadPool.o osqueue.o osqueue.h threadPool.h
	ar rcs libthreadPool.a threadPool.o osqueue.o

clean:
	rm -f libthreadPool.a threadPool.o osqueue.o a.out our_tester