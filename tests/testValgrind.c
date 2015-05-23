#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "osqueue.h"
#include "threadPool.h"


/******************************************************************************/
/**************************[TASKS FUNCTIONS START]*****************************/
/******************************************************************************/

typedef struct awesomeContainer
{
   int awesomeNum;
   char* awesomeString;
}AwesomeContainer;

void printStart()
{
   char* start = " _______ _______ _______ ______ _______ \n|       |       |   _   |    _ |       |\n|  _____|_     _|  |_|  |   | ||_     _|\n| |_____  |   | |       |   |_||_|   |  \n|_____  | |   | |       |    __  |   |  \n _____| | |   | |   _   |   |  | |   |  \n|_______| |___| |__| |__|___|  |_|___|  ";
   char* test = " _______ _______ _______ _______ \n|       |       |       |       |\n|_     _|    ___|  _____|_     _|\n  |   | |   |___| |_____  |   |  \n  |   | |    ___|_____  | |   |  \n  |   | |   |___ _____| | |   |  \n  |___| |_______|_______| |___|  ";
   printf("%s\n", test);
   printf("%s\n", start);
}

void printEnd()
{
   char* end = " _______ __    _ ______  \n|       |  |  | |      | \n|    ___|   |_| |  _    |\n|   |___|       | | |   |\n|    ___|  _    | |_|   |\n|   |___| | |   |       |\n|_______|_|  |__|______| ";
   char* test = " _______ _______ _______ _______ \n|       |       |       |       |\n|_     _|    ___|  _____|_     _|\n  |   | |   |___| |_____  |   |  \n  |   | |    ___|_____  | |   |  \n  |   | |   |___ _____| | |   |  \n  |___| |_______|_______| |___|  ";
   printf("%s\n", test);
   printf("%s\n", end);
}

void simpleTask(void* a)
{
   int j;
   int i;
   for (i = 0; i < 5; ++i)
   {
      j=i;
   }
   if (a==NULL)
   {
      return;
   }
   AwesomeContainer* con = (AwesomeContainer*)(a);
   con->awesomeNum = 1; // for success
}


void badfunction(void *a)
{
   //this is a function that should not run,
   //i will only insert it after a destroy was called
   assert(0==1);
}

void printOK()
{
   printf("\n");
   char* line1 = " ___                        ____  _  __                      ___"; 
   char* line2 = "|  _|                      / __ \\| |/ /                     |_  |";
   char* line3 = "| |                       | |  | | ' /                        | |";
   char* line4 = "| |          TEST         | |  | |  <           PASSED        | |";
   char* line5 = "| |                       | |__| | . \\                        | |";
   char* line6 = "| |_                       \\____/|_|\\_\\                      _| |";
   char* line7 = "|___|                                                       |___|";
   
   char* text[7] = {line1,line2,line3,line4,line5,line6,line7}; 

   int i;
   //print text
   for (i = 0; i < 7; ++i)
   {
      printf("%s\n",text[i]);
   } 
   printf("\n");
}



/******************************************************************************/
/***************************[TASKS FUNCTIONS END]******************************/
/******************************************************************************/


void test_create_and_destroy()
{
   ThreadPool* tp1 = tpCreate(3);
   tpDestroy(tp1,1);

   ThreadPool* tp2 = tpCreate(3);
   tpDestroy(tp2,0);

   printOK();
   printf(" \n");
}

void test_thread_pool_sanity()
{
   int i;
   
   ThreadPool* tp = tpCreate(3);
   for(i=0; i<5; ++i)
   {
      tpInsertTask(tp,simpleTask,NULL);
   }
   
   tpDestroy(tp,1);
   printOK();
   printf(" \n");
}

void test_single_thread_many_tasks()
{
   ThreadPool* tp = tpCreate(1);

   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   
   tpDestroy(tp,1);
   printOK();
   printf(" \n");
}

void test_many_threads_single_task()
{
   ThreadPool* tp = tpCreate(50);
   AwesomeContainer con;
   con.awesomeNum = 0;
   con.awesomeString = NULL; // we use only the awesomeNum
   tpInsertTask(tp,simpleTask,&con);

   tpDestroy(tp,1);

   assert(con.awesomeNum==1);
   printOK();
   printf(" \n");
}

void test_destroy_should_wait_for_tasks_1()
{
   ThreadPool* tp = tpCreate(10);

   AwesomeContainer con;
   con.awesomeNum = 0;
   con.awesomeString = "DontCare"; // we use only the awesomeNum
   tpInsertTask(tp,simpleTask,&con);

   tpDestroy(tp,1);

   assert(con.awesomeNum==1);
   printOK();
   printf(" \n");
}

void test_destroy_should_wait_for_tasks_2()
{
   ThreadPool* tp = tpCreate(5);

   AwesomeContainer con;
   con.awesomeNum = 0;
   con.awesomeString = "DontCare"; // we use only the awesomeNum
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,NULL);
   tpInsertTask(tp,simpleTask,&con);

   tpDestroy(tp,1);

   assert(con.awesomeNum==1);
   printOK();
   printf(" \n");
}

void test_destroy_should_not_wait_for_tasks()
{
   ThreadPool* tp = tpCreate(4);

      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);
      tpInsertTask(tp,simpleTask,NULL);

   tpDestroy(tp,0);
   printOK();
   printf(" \n");
}


void aux_test_for_agressive(int num)
{
   int i;
   
   ThreadPool* tp = tpCreate(num);
   for(i=0; i<num; ++i)
   {
      tpInsertTask(tp,simpleTask,NULL);
   } 
   tpDestroy(tp,1);
   int precent = num*5;
   char bar[43];
   int j;
   for (j = 0; j < 42; j++)
   {
      if (j==0) {
         bar[j] = '[';
      } else if (j>=1 && j<=num*2){
         bar[j] = '=';
      } else if (j>num*2 && j<=40){
         bar[j] = ' ';
      } else {
         bar[j] = ']';
      }
   }
   bar[42] = '\0';
   printf("%d%% %s\n",precent, bar);
   printf("\033[1A");
   printf("\r");
}
void test_agressive()
{
   //repeat the same test many times to check for rare cases
   int i;
   for (i = 1; i <= 20; ++i)
   {
      aux_test_for_agressive(i);
   }
   printOK();
   printf(" \n");
}


int main()
{
   printStart();
   printf("\n");
   

   printf("test_create_and_destroy...\n");
   test_create_and_destroy();


   printf("test_thread_pool_sanity...\n");
   test_thread_pool_sanity();


   printf("test_agressive, this might take a while...\n");
   test_agressive();


   printf("test_single_thread_many_tasks...\n");
   test_single_thread_many_tasks();


   printf("test_many_threads_single_task...\n");
   test_many_threads_single_task();


   printf("test_destroy_should_wait_for_tasks #1 (more threads then tasks)...\n");
   test_destroy_should_wait_for_tasks_1();


   printf("test_destroy_should_wait_for_tasks #2 (more tasks then threads)...\n");
   test_destroy_should_wait_for_tasks_2();


   printf("test_destroy_should_not_wait_for_tasks...\n");
   test_destroy_should_not_wait_for_tasks();


   printEnd();
   return 0;
}
