#ifndef _THPOOL_
#define _THPOOL_

//typedef struct ThreadPoolManager ThreadPoolManager;

/* Task */
typedef struct task{
	void* (*function)(void* arg);
	void* arg;
    struct task* next;
} Task;


/* Task queue */
typedef struct taskqueue{
	pthread_mutex_t qlock;
	Task  *front;
	Task  *end;
	int   len;
} Taskqueue;


/* Threadpool */
typedef struct ThreadPoolManager{
	pthread_t*  threads;                  /* pointer to threads        */
	pthread_mutex_t  plock;       /* used for thread count etc */
	pthread_cond_t  pcond;    /* signal to ThreadPoolManagerwait     */
	Taskqueue  taskqueue;                  /* job queue                 */
	int num_threads_working;
    int total_threads;
	int* sockets;
} ThreadPoolManager;

int ThreadPoolInit(ThreadPoolManager *th_pool, int num_threads);

int ThreadPoolInsertTask(ThreadPoolManager *th_pool, void* (*function_p)(void* arg), void* arg_p);

void ThreadPoolManagerdestroy(ThreadPoolManager* ThreadPoolManagerp);

#endif
