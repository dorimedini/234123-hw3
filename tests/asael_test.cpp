#include <iostream>
#include <pthread.h>
#include <sstream>
#include <semaphore.h>
#include <time.h>
#include <string.h>

using namespace std;

struct ThreadPool;
extern "C" {
	ThreadPool* tpCreate(int numOfThreads);
	void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);
	int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);
}

string itos(int i) {
	stringstream buff;
	string str;
	buff<<i;
	buff>>str;
	return str;
}

struct task {
	sem_t sem;
	string name;
};

__thread char my_name[16];

string thread_name() {
	return my_name;
}

void manuall(void* a) {
	task *t=(task*)a;
	cout<<"beginning task "<<t->name<<" on "<<thread_name()<<endl;
	sem_wait(&t->sem);
	cout<<"ending task "<<t->name<<" on "<<thread_name()<<endl;
}

ThreadPool* tp;

pthread_barrier_t setname_barrier;
void setname(void* a) {
	string str="Thread "+itos(long(a));
	strcpy(my_name,str.c_str());
	pthread_barrier_wait(&setname_barrier);
}

void hello(void* a) {
	cout<<"hello from "<<thread_name()<<endl;
}

void *destroy(void *a) {
	tpDestroy(tp,int(long(a)));
	cout<<"End of destroy()"<<endl;
	return NULL;
}

void init(int num) {
	tp = tpCreate(num);
	pthread_barrier_init(&setname_barrier,NULL,num); // memory leaks here!
	for (int i=0;i<num;i++) tpInsertTask(tp,setname,(void*)long(i));
}

struct timespec req={0,100000000l}; //100ms

task tasks[500];
void main_loop() {
	string inp;
	while (cin>>inp) {
		if (inp=="continue" || inp=="c") {
			int id;
			cin>>id;
			sem_post(&tasks[id].sem);
		} else if (inp=="insert" || inp=="i") {
			int id;
			cin>>id;
			tpInsertTask(tp,manuall,&tasks[id]);
		} else if (inp=="hello" || inp=="h") {
			tpInsertTask(tp,hello,NULL);
		} else if (inp=="destroy" || inp=="d") {
			long w;
			cin>>w;
			pthread_t t;
			pthread_create(&t,NULL,destroy,(void*)w);
		} else if (inp=="init") {
			int num;
			cin>>num;
			init(num);
		}
		nanosleep(&req,NULL); //let our threads do something, before get EOF
	}
}

int main() {
	for (int i=0;i<500;i++) {
		sem_init(&tasks[i].sem,0,0);
		tasks[i].name="Task ";
		tasks[i].name+=itos(i);
	}
	main_loop();
}
