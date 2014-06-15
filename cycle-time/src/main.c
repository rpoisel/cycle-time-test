#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <signal.h>

#define MY_PRIORITY (95) /* we use 95 as the PRREMPT_RT use 50
                            as the priority of kernel tasklets
                            and interrupt handler by default */

#define MAX_SAFE_STACK (8*1024) /* The maximum stack size which is
                                   guaranteed safe to access without
                                   faulting */

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

#define INTERVAL 1000000 /* 1ms*/
#define BUSY_COUNT 10000

/* globals */
long long cnt = 0;
long long deviation_max = 0;
long long deviation_min = LLONG_MAX;
double deviation_avg = 0;

static void stack_prefault(void) 
{

    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
    return;
}

static void actual_job(const struct timespec* time_act,
        const struct timespec* time_last);
static void signal_handler(int sig);

#if 0
int main(int argc, char* argv[])
#else
int main(void)
#endif
{
    struct timespec t;
    struct timespec t_actual;
    struct timespec t_last = { 0, 0 };
    struct sched_param param;

    /* Declare ourself as a real time task */
    param.sched_priority = MY_PRIORITY;
    if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) 
    {
        perror("sched_setscheduler failed");
        exit(-1);
    }

    /* Lock memory */
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) 
    {
        perror("mlockall failed");
        exit(-2);
    }

    /* set signal handlers */
    if (signal(SIGINT, signal_handler) == SIG_ERR
            || signal(SIGTERM, signal_handler) == SIG_ERR)
    {
        fprintf(stderr, "Could not install signal handler for SIGINT.\n");
        return 1;
    }

    /* Pre-fault our stack */
    stack_prefault();

    clock_gettime(CLOCK_MONOTONIC ,&t);
    /* start after one second */
    t.tv_sec++;

    for(;;)
    {
        /* wait until next shot */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        clock_gettime(CLOCK_MONOTONIC, &t_actual);

        /* do the stuff */
        actual_job(&t_actual, &t_last);

        /* calculate next shot */
        t.tv_nsec += INTERVAL;

        while (t.tv_nsec >= NSEC_PER_SEC) 
        {
            t.tv_nsec -= NSEC_PER_SEC;
            t.tv_sec++;
        }
        t_last = t_actual;
    }
}

static void actual_job(const struct timespec* time_act,
        const struct timespec* time_last)
{
    volatile int cnt_busy = 0;
    long long deviation_cur = 0;

    if (time_last->tv_sec != 0 || time_last->tv_nsec != 0)
    {
        deviation_cur = ((((long long)time_act->tv_sec * NSEC_PER_SEC + time_act->tv_nsec) - ((long long)time_last->tv_sec * NSEC_PER_SEC + time_last->tv_nsec)) - INTERVAL) / 1000;

        if (llabs(deviation_cur) < llabs(deviation_min))
        {
            deviation_min = deviation_cur;
        }

        if (llabs(deviation_cur) > llabs(deviation_max))
        {
            deviation_max = deviation_cur;
        }

    }
    for (cnt_busy = 0; cnt_busy < BUSY_COUNT; cnt_busy++)
    {
        /* cannot be optimized because of cnt's volatility */
    }

    if (cnt == 0)
    {
        deviation_avg = llabs(deviation_cur);
    }
    else
    {
        deviation_avg = ((double)cnt * deviation_avg) / (cnt + 1) + 
            llabs(deviation_cur) / ((double)cnt + 1);
    }

    cnt++;
}

static void signal_handler(int sig)
{
    int ret = EXIT_SUCCESS;

    fprintf(stdout, "Count: %lld Min: %lld Max: %lld Avg: %f\n",
            cnt,
            deviation_min,
            deviation_max,
            deviation_avg);

    exit(ret);
}
