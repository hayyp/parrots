#include <pthread.h>
#include <stdlib.h>
#include "tpool.h"

#ifdef DE_BUG
#include <stdio.h>
#endif

static void *tpool_worker(void *arg);
static void  tpool_job_destroy(tpool_job_t *job);
static tpool_job_t *tpool_job_get(tpool_t *tpool);
static tpool_job_t *tpool_job_create(thr_func_t func, void *arg);

tpool_t *tpool_create(int num)
{
#ifdef DE_BUG
    perror("tp_create");
#endif
    tpool_t     *tpool;
    pthread_t   thread;

    if (num == 0) num = 2;

    tpool = (tpool_t *) malloc(sizeof(tpool_t));
    tpool->thread_cnt = num;
    tpool->stopped = 0;

    pthread_mutex_init(&(tpool->work_mutex), NULL);
    pthread_cond_init(&(tpool->work_cond), NULL);
    pthread_cond_init(&(tpool->working_cond), NULL);

    tpool->jobq_head = NULL;
    tpool->jobq_tail  = NULL;

    for (int i = 0; i < num; ++i) {
        pthread_create(&thread, NULL, tpool_worker, tpool);
        pthread_detach(thread); // no need to wait
    }

    return tpool;
}

void tpool_destroy(tpool_t *tpool)
{
#ifdef DE_BUG
    perror("tp_destroy");
#endif
    tpool_job_t *job;
    tpool_job_t *job2;

    if (tpool == NULL) return;

    pthread_mutex_lock(&(tpool->work_mutex));
    job = tpool->jobq_head;
    while (job != NULL) {
        job2 = job->next;
        tpool_job_destroy(job);
        job = job2;
    }

    tpool->stopped = 1;
    pthread_cond_broadcast(&(tpool->work_cond));
    pthread_mutex_unlock(&(tpool->work_mutex));

    tpool_wait(tpool);

    pthread_mutex_destroy(&(tpool->work_mutex));
    pthread_cond_destroy(&(tpool->work_cond));
    pthread_cond_destroy(&(tpool->working_cond));

    free(tpool);
}

int tpool_add_job(tpool_t *tpool, thr_func_t func, void *arg)
{
#ifdef DE_BUG
    perror("tp_add_job");
#endif
    tpool_job_t *job;

    if (tpool == NULL) return -1;

    job = tpool_job_create(func, arg);
    if (job == NULL) return -1; 

    pthread_mutex_lock(&(tpool->work_mutex));
    if (tpool->jobq_head == NULL) {
        tpool->jobq_head = job;
        tpool->jobq_tail = tpool->jobq_head;
    } else {
        tpool->jobq_tail->next = job;
        tpool->jobq_tail       = job;
    }

    pthread_cond_broadcast(&(tpool->work_cond));
    pthread_mutex_unlock(&(tpool->work_mutex));

    return 0;
}

void tpool_wait(tpool_t *tpool)
{
#ifdef DE_BUG
    perror("tp_wait");
#endif
    if (tpool == NULL) return;

    pthread_mutex_lock(&(tpool->work_mutex));
    while (1) {
        if ((!tpool->stopped && tpool->working_cnt != 0) || (tpool->stopped && tpool->thread_cnt != 0)) {
            /* notified whenever there is no jobs to do, and recheck the condition above */
            pthread_cond_wait(&(tpool->working_cond), &(tpool->work_mutex));
        } else {
            break;
        }
    }

    pthread_mutex_unlock(&(tpool->work_mutex));
}


/*
 * Static functions
 */



static tpool_job_t *tpool_job_create(thr_func_t func, void *arg)
{
#ifdef DE_BUG
    perror("tp_job_create");
#endif
    tpool_job_t *job;

    if (func == NULL) return NULL;

    job = (tpool_job_t *) malloc(sizeof(tpool_job_t));
    job->func = func;
    job->arg  = arg;
    job->next = NULL;

    return job;
}

static void tpool_job_destroy(tpool_job_t *job)
{
#ifdef DE_BUG
    perror("tp_job_destroy");
#endif
    if (job == NULL) return;
    free(job);
}

static tpool_job_t *tpool_job_get(tpool_t *tpool)
{
#ifdef DE_BUG
    perror("tp_job_get");
#endif
    tpool_job_t *job;

    if (tpool == NULL) return NULL;

    job = tpool->jobq_head;
    if (job == NULL) return NULL;

    if (job->next == NULL) {
        tpool->jobq_head = NULL;
        tpool->jobq_tail = NULL;
    } else {
        tpool->jobq_head = job->next;
    }

    return job;
}

static void *tpool_worker(void *arg)
/* what each thread will actually do */
{
#ifdef DE_BUG
    perror("tp_worker");
#endif
    tpool_t      *tpool = arg;
    tpool_job_t *job;

    while (1) {
        pthread_mutex_lock(&(tpool->work_mutex));
        while (tpool->jobq_head == NULL && !tpool->stopped)
            pthread_cond_wait(&(tpool->work_cond), &(tpool->work_mutex));

        if (tpool->stopped) break;

        job = tpool_job_get(tpool);
        tpool->working_cnt++;
        pthread_mutex_unlock(&(tpool->work_mutex));

        if (job != NULL) {
            job->func(job->arg);
            tpool_job_destroy(job);
        }

        pthread_mutex_lock(&(tpool->work_mutex));
        tpool->working_cnt--;
        if (!tpool->stopped && tpool->working_cnt == 0 && tpool->jobq_head == NULL)
            pthread_cond_signal(&(tpool->working_cond));
        pthread_mutex_unlock(&(tpool->work_mutex));
    }

    tpool->thread_cnt--;
    pthread_cond_signal(&(tpool->working_cond));
    pthread_mutex_unlock(&(tpool->work_mutex));

    return NULL;
}
