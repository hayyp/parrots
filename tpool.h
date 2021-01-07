#ifndef TPOOL_H
#define TPOOL_H

typedef void (*thr_func_t)(void *arg);

struct tpool_job {
    thr_func_t        func;
    void              *arg;
    struct tpool_job *next;
};

typedef struct tpool_job tpool_job_t;

struct tpool {
    tpool_job_t    *jobq_head;
    tpool_job_t    *jobq_tail;

    pthread_mutex_t work_mutex;   // counting jobs, others should wait
    pthread_cond_t  work_cond;    // there are new jobs
    pthread_cond_t  working_cond;
    size_t          working_cnt;
    size_t          thread_cnt;
    int             stopped;
};

typedef struct tpool tpool_t;

/**
 * @brief Initialize a threadpool
 * 
 * @example
 *    
 *     ..
 *     tpool_t *tpool = tpool_create(4);
 *     ..
 *
 * @param  num,       that is, the number of threads you want to create
 * @return tpool_t *, that is, a pointer to the threadpool structure
 *
 */

tpool_t *tpool_create(int num);

/**
 * @brief Destroy a threadpool
 *
 * @example
 *
 *      ..
 *      tpool_destroy(tpool);
 *      ..
 *
 * @param tpool, a pointer to the threadpool structure returned by tpool_create
 * @return nothing
 *
 */

void    tpool_destroy(tpool_t *tpool);

/**
 * @brief Add work to the job queue
 *
 * @example
 *
 *      ..
 *      void echo(int a) {
 *          printf("%d\n", a)
 *      }
 *
 *      int main() {
 *      ..
 *          int num = 1;
 *          int err = tpool_add_job(tpool, add, &num);
 *      ..
 *      }
 * @param   tpool       a pointer to the threadpool
 * @param   thr_func_t  a function pointer
 * @param   arg         argument(s) can be passed as pointers
 * @return  0 for success and -1 otherwise
 */

int     tpool_add_job(tpool_t *tpool, thr_func_t func, void *arg);

/**
 * @brief Wait for all queued tasks to finish
 * @example
 *
 *      ..
 *      tpool_t *tpool = tpool_create(4);
 *      ..
 *      int err = tpool_add_job(tpool, func, arg);
 *      ..
 *      tpool_wait(tpool);
 *      ..
 *
 * @param   tpool   a pointer to the threadpool
 * @return nothing
 */

void    tpool_wait(tpool_t *tpool);

#endif
