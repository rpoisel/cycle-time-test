#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>

#define MY_PRIORITY (49) /* we use 49 as the PRREMPT_RT use 50
                            as the priority of kernel tasklets
                            and interrupt handler by default */

#define MAX_SAFE_STACK (8*1024) /* The maximum stack size which is
                                   guaranteed safe to access without
                                   faulting */

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

#define INTERVAL 1000000 /* 1ms*/
#define BUSY_COUNT 10000

static void stack_prefault(void) 
{

    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
    return;
}

static void actual_job(const struct timespec* time_act,
        const struct timespec* time_last);

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
    volatile int cnt = 0;

    if (time_last->tv_sec != 0 || time_last->tv_nsec != 0)
    {
        fprintf(stdout, "Tick tick ...%f(us)\n",
                ((((long long)time_act->tv_sec * NSEC_PER_SEC + time_act->tv_nsec) - ((long long)time_last->tv_sec * NSEC_PER_SEC + time_last->tv_nsec)) - (double)INTERVAL) / 1000);
    }
    for (cnt = 0; cnt < BUSY_COUNT; cnt++)
    {
        /* cannot be optimized because of cnt's volatile nature */
    }
}
