#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#define MY_PRIORITY (95) /* we use 95 as the PRREMPT_RT use 50
                            as the priority of kernel tasklets
                            and interrupt handler by default */

#define MAX_SAFE_STACK (8*1024) /* The maximum stack size which is
                                   guaranteed safe to access without
                                   faulting */

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

#define INTERVAL 1000000 /* 1ms*/
#define BUSY_COUNT 10000

/* type definitions */
struct realtime_data
{
    long long cnt;
    long long deviation_max;
    long long deviation_min;
    double deviation_avg;
};

struct job_pars
{
    const struct timespec const* time_act;
    const struct timespec const* time_last;
    struct realtime_data* rt_data;
};

/* globals */
int flag = 1;
pthread_t thread_id_comm;

static void stack_prefault(void) 
{

    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
    return;
}

static void* cyclist(void* thr_par);
static void* communicator(void* thr_par);
static void actual_job(void* job_par);
static void signal_handler(int sig);

#if 0
int main(int argc, char* argv[])
#else
int main(void)
#endif
{
    struct realtime_data rt_data =
    {
        .cnt = 0,
        .deviation_max = 0,
        .deviation_min = LLONG_MAX,
        .deviation_avg = 0
    };
    pthread_t thread_id_cyclist;

    /* Lock memory */
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) 
    {
        perror("mlockall failed");
        return EXIT_FAILURE;
    }

    /* set signal handlers */
    if (signal(SIGINT, signal_handler) == SIG_ERR
            || signal(SIGTERM, signal_handler) == SIG_ERR)
    {
        fprintf(stderr, "Could not install signal handler for SIGINT.\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread_id_cyclist, NULL, cyclist, &rt_data) != 0)
    {
        fprintf(stderr, "Could not instantiate cyclist thread.\n");
        return EXIT_FAILURE;
    }
    if (pthread_create(&thread_id_comm, NULL, communicator, &rt_data) != 0)
    {
        fprintf(stderr, "Could not instantiate communication thread.\n");
        return EXIT_FAILURE;
    }

    if (pthread_join(thread_id_cyclist, NULL) != 0)
    {
        fprintf(stderr, "Could not join cyclist thread.\n");
        return EXIT_FAILURE;
    }
    if (pthread_join(thread_id_comm, NULL) != 0)
    {
        fprintf(stderr, "Could not join comm thread.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void* cyclist(void* thr_par)
{
    struct timespec t;
    struct timespec t_actual;
    struct timespec t_last = { 0, 0 };
    struct job_pars pars =
    {
        .time_act  = &t_actual,
        .time_last = &t_last,
        .rt_data = (struct realtime_data*)thr_par
    };
    struct sched_param param;

    /* Declare ourself as a real time task */
    param.sched_priority = MY_PRIORITY;
    if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == -1)
    {
        perror("sched_setscheduler failed");
        exit(-1);
    }

    /* Pre-fault our stack */
    stack_prefault();

    clock_gettime(CLOCK_MONOTONIC ,&t);
    /* start after one second */
    t.tv_sec++;

    for(; flag;)
    {
        /* wait until next shot */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        clock_gettime(CLOCK_MONOTONIC, &t_actual);

        /* do the stuff */
        actual_job(&pars);

        /* calculate next shot */
        t.tv_nsec += INTERVAL;

        while (t.tv_nsec >= NSEC_PER_SEC) 
        {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
        t_last = t_actual;
    }
    return NULL;
}

static void actual_job(void* job_par)
{
    struct job_pars* j_p = (struct job_pars*)job_par;
    volatile int cnt_busy = 0;
    long long deviation_cur = 0;

    if (j_p->time_last->tv_sec != 0 || j_p->time_last->tv_nsec != 0)
    {
        deviation_cur =
            ((((long long)j_p->time_act->tv_sec * NSEC_PER_SEC + j_p->time_act->tv_nsec) -
              ((long long)j_p->time_last->tv_sec * NSEC_PER_SEC + j_p->time_last->tv_nsec)) -
             INTERVAL) /
            1000;

        if (llabs(deviation_cur) < llabs(j_p->rt_data->deviation_min))
        {
            j_p->rt_data->deviation_min = deviation_cur;
        }

        if (llabs(deviation_cur) > llabs(j_p->rt_data->deviation_max))
        {
            j_p->rt_data->deviation_max = deviation_cur;
        }

    }
    for (cnt_busy = 0; cnt_busy < BUSY_COUNT; cnt_busy++)
    {
        /* cannot be optimized because of cnt's volatility */
    }

    if (j_p->rt_data->cnt == 0)
    {
        j_p->rt_data->deviation_avg = llabs(deviation_cur);
    }
    else
    {
        j_p->rt_data->deviation_avg =
            ((double)j_p->rt_data->cnt * j_p->rt_data->deviation_avg) /
            (j_p->rt_data->cnt + 1) +
            llabs(deviation_cur) / ((double)j_p->rt_data->cnt + 1);
    }

    j_p->rt_data->cnt++;
}

static void* communicator(void* thr_par)
{
    const struct realtime_data const* rt_data = (const struct realtime_data const*)thr_par;
    for (;;)
    {
        if (rt_data->cnt > 1)
        {
            fprintf(stdout, "Count: %lld Min: %lld Max: %lld Avg: %f\n",
                    rt_data->cnt,
                    rt_data->deviation_min,
                    rt_data->deviation_max,
                    rt_data->deviation_avg);
        }

        sleep(1);
    }
    return NULL;
}

static void signal_handler(int sig)
{
    flag = 0;
    pthread_cancel(thread_id_comm);
}
