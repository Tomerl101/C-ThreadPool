#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/prctl.h>
#include "thpool.h"

static void* thread_loop(void* thread_pool);

static int   Taskqueue_init(Taskqueue* taskqueue_p);
static void  Taskqueue_clear(Taskqueue* taskqueue_p);
static void  Taskqueue_push(Taskqueue* taskqueue_p, Task* newTask_p);
static struct task* Taskqueue_pull(Taskqueue* taskqueue_p);
static void  Taskqueue_destroy(Taskqueue* taskqueue_p);

/* ========================== THREADPOOL ============================ */

/* Initialise thread pool */
int ThreadPoolInit(struct ThreadPoolManager* ThreadPoolManagerp,int num_threads){

    int i = 0;
	if (num_threads < 0){
		num_threads = 0;
	}

    ThreadPoolManagerp->total_threads = num_threads;

	/* Initialise the task queue */
	if (Taskqueue_init(&ThreadPoolManagerp->taskqueue) == -1){
		perror("Could not allocate memory for task queue...\n");
		free(ThreadPoolManagerp);
		return -1;
	}

	/* Make threads in pool */
	ThreadPoolManagerp->threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
	if (ThreadPoolManagerp->threads == NULL){
		perror("Could not allocate memory for threads...\n");
		Taskqueue_destroy(&ThreadPoolManagerp->taskqueue);
		free(ThreadPoolManagerp);
		return -1;
	}
	pthread_mutex_init(&(ThreadPoolManagerp->plock), NULL);
	pthread_cond_init(&ThreadPoolManagerp->pcond, NULL);

	/* Thread init */
	int n;
	for (n=0; n<num_threads; n++){
        if (pthread_create(&ThreadPoolManagerp->threads[n], NULL, thread_loop, ThreadPoolManagerp))
            return -1;
	}
    printf("Done initialize ThreadPool...\n");
	return 0;
}


/* Add work to the thread pool */
int ThreadPoolInsertTask(ThreadPoolManager* ThreadPoolManagerp, void* (*function_p)(void* arg), void* arg_p)
{
	Task* newTask;

	newTask=(struct task*)malloc(sizeof(struct task));
	if (newTask==NULL){
		perror("Could not allocate memory for new job\n...");
		return -1;
	}

	/* add function and argument */
	newTask->function=function_p;
	newTask->arg=arg_p;

	/* add job to queue */
	Taskqueue_push(&ThreadPoolManagerp->taskqueue, newTask);
    pthread_cond_signal(&ThreadPoolManagerp->pcond);

	return 0;
}


/* Destroy the threadpool */
void ThreadPoolManagerdestroy(ThreadPoolManager* ThreadPoolManagerp){
	if (ThreadPoolManagerp == NULL) return ;
    int num_threads = ThreadPoolManagerp->total_threads;
	/* Give one second to kill idle threads */
	 double TIMEOUT = 2.0;
	 time_t start, end;
	 double tpassed = 0.0;
	 time (&start);
	 while (tpassed < TIMEOUT){
	 	time (&end);
	 	tpassed = difftime(end,start);
	 }

	/* Job queue cleanup */
	Taskqueue_destroy(&ThreadPoolManagerp->taskqueue);
	/* Deallocs */
	 int n;
     int total_threads = ThreadPoolManagerp->total_threads;
	 for (n=0; n < total_threads; n++){
         pthread_cancel(ThreadPoolManagerp->threads[n]);
	 }
	free(ThreadPoolManagerp->threads);
	
    pthread_mutex_destroy(&ThreadPoolManagerp->plock);
    pthread_cond_destroy(&ThreadPoolManagerp->pcond);
}


static void* thread_loop(void* thread_pool){
	/* Assure all threads have been created before starting serving */
	ThreadPoolManager* ThreadPoolManagerp = (struct ThreadPoolManager*)thread_pool;

	while(1){
        pthread_mutex_lock(&ThreadPoolManagerp->plock);
        while(ThreadPoolManagerp->taskqueue.len == 0){
            pthread_cond_wait(&ThreadPoolManagerp->pcond, &ThreadPoolManagerp->plock);
        }

        ThreadPoolManagerp->num_threads_working++;
        pthread_mutex_unlock(&ThreadPoolManagerp->plock);

        /* Read task from queue and execute it */
        void* (*func_p)(void* arg);
        void*  arg_p;
        struct task* task_p;
        task_p = Taskqueue_pull(&ThreadPoolManagerp->taskqueue);
        if (task_p) {
            func_p = task_p->function;
            arg_p  = task_p->arg;
            func_p(arg_p);
			free(task_p->arg);
            free(task_p);
        }

        pthread_mutex_lock(&ThreadPoolManagerp->plock);
        ThreadPoolManagerp->num_threads_working--;
        pthread_mutex_unlock(&ThreadPoolManagerp->plock);
	}
	return NULL;
}


/* ============================ TASK QUEUE =========================== */


/* Initialize queue */
static int Taskqueue_init(Taskqueue* taskqueue_p){
	taskqueue_p->len = 0;
	taskqueue_p->front = NULL;
	taskqueue_p->end  = NULL;

	pthread_mutex_init(&(taskqueue_p->qlock), NULL);

	return 0;
}


/* Clear the queue */
static void Taskqueue_clear(Taskqueue* taskqueue_p){

	while(taskqueue_p->len){
		free(Taskqueue_pull(taskqueue_p));
	}

	taskqueue_p->front = NULL;
	taskqueue_p->end  = NULL;
	taskqueue_p->len = 0;
}


/* Add new task to the queue */
static void Taskqueue_push(Taskqueue* taskqueue_p, Task* newTask_p){

	pthread_mutex_lock(&taskqueue_p->qlock);
    newTask_p->next = NULL;

	switch(taskqueue_p->len){

		case 0:  /* if no jobs in queue */
					taskqueue_p->front = newTask_p;
					taskqueue_p->end  = newTask_p;
					break;

		default: /* if jobs in queue */
                    taskqueue_p->end->next = newTask_p;
					taskqueue_p->end = newTask_p;

	}
	taskqueue_p->len++;

	pthread_mutex_unlock(&taskqueue_p->qlock);
}


static struct task* Taskqueue_pull(Taskqueue* taskqueue_p){

	pthread_mutex_lock(&taskqueue_p->qlock);
	Task* task_p = taskqueue_p->front;

	switch(taskqueue_p->len){

		case 0:  /* if no task in queue */
		  			break;

		case 1:  /* if one task in queue */
					taskqueue_p->front = NULL;
					taskqueue_p->end  = NULL;
					taskqueue_p->len = 0;
					break;

		default: /* if >1 task in queue */
                    taskqueue_p->front = task_p->next;
					taskqueue_p->len--;

	}

	pthread_mutex_unlock(&taskqueue_p->qlock);
	return task_p;
}


/* Free all queue resources back to the system */
static void Taskqueue_destroy(Taskqueue* taskqueue_p){
	Taskqueue_clear(taskqueue_p);
    pthread_mutex_destroy(&taskqueue_p->qlock);
}
