all: test.c libthreadPool.a osqueue.h threadPool.h
	gcc -L. test.c -lthreadPool -lpthread -o a.out
	
osqueue.o: osqueue.c
	gcc -c -L. osqueue.c -lpthread

threadPool.o: osqueue.o threadPool.c osqueue.h
	gcc -c -L. threadPool.c -lpthread
	
libthreadPool.a: threadPool.o osqueue.o osqueue.h threadPool.h
	ar rcs libthreadPool.a threadPool.o osqueue.o

clean:
	rm -f libthreadPool.a threadPool.o osqueue.o a.out