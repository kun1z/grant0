#define TOTAL_TARGET 4000000000000
#define AWS_S3_BUCKET_NAME "your_bucket_name"
//----------------------------------------------------------------------------------------------------------------------
#include <assert.h>
static_assert(sizeof(double) == 8, "This code requires [double] to be exactly 8 bytes.");
//----------------------------------------------------------------------------------------------------------------------
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>
//----------------------------------------------------------------------------------------------------------------------
#include "miniutil.h"
#include "blake2bmod.h"
//----------------------------------------------------------------------------------------------------------------------
pthread_spinlock_t csoutput, csexec, cstotals;
typedef struct
{
    uint64_t buffer[8], counter, even;
    int p;
}thread_data;
uint64_t total_counter, total_even;
//----------------------------------------------------------------------------------------------------------------------
void remix_rng(thread_data * const);
uint64_t rng64(thread_data * const);
double rng0_1(thread_data * const);
void * termination_notice(void *);
void * thread(void *);
//----------------------------------------------------------------------------------------------------------------------
int main(void)
{
    char buf[64];
    pthread_spin_init(&csoutput, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&csexec, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cstotals, PTHREAD_PROCESS_PRIVATE);

    if (!system(0))
    {
        o("FATAL: Shell is not available\n");
        return 1;
    }

    if (!e("type curl &> /dev/null"))
    {
        o("INFO: Monitoring for termination\n");
        pthread_t term;
        pthread_create(&term, 0, termination_notice, 0);
    }

    const int threads = get_nprocs();
    pthread_t ht[threads];

    o("%s > Starting\nThreads: %d\n", datetime(buf), threads);

    for (uintptr_t i=0;i<threads;i++)
    {
        pthread_create(&ht[i], 0, thread, (void *)i);
    }

    for (int i=0;i<threads;i++)
    {
        pthread_join(ht[i], 0);
    }

    o("Total Samples: %lu\nTotal Evens: %lu\nFinal Ratio: %.16f\n", total_counter, total_even, total_even / (double)total_counter);

    o("%s > Ending\n\n", datetime(buf));

    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
void * thread(void * tID)
{
    const uintptr_t temp = (uintptr_t)tID;
    const int id = temp;

    thread_data * const td = calloc(1, sizeof(thread_data));
    if (!td)
    {
        o("FATAL: thread_data * const td = calloc(1, sizeof(thread_data));\n");
        exit(1);
    }

    remix_rng(td);

    o("Starting thread: %d\n", id);

    volatile const uint64_t start_tick = tick();

    while (1)
    {
        const double x = rng0_1(td);
        const double y = rng0_1(td);

        const uint64_t result = floor(x / y);

        if (!(result & 1))
        {
            td->even++;
        }

        if (!(++td->counter & 0xFFFFFFF))
        {
            remix_rng(td);

            pthread_spin_lock(&cstotals);
            total_counter += td->counter;
            total_even += td->even;
            volatile const uint64_t local_counter = total_counter;
            volatile const uint64_t local_evens = total_even;
            pthread_spin_unlock(&cstotals);

            td->counter = 0;
            td->even = 0;

            if (!id)
            {
                const double fsecs = (tick() - start_tick) / 1000.;
                const double mps = (local_counter / fsecs) / 1000000.;
                o("Samples: %.1f billion (%.1f mps) - result: %.16f\n", local_counter / 1000000000., mps, local_evens / (double)local_counter);
            }

            if (local_counter >= TOTAL_TARGET) break;
        }
    }

    volatile const uint64_t end_tick = tick();

    const double mins = (end_tick - start_tick) / 60000.0;
    o("t%d> Thread terminating after %.1f minutes\n", id, mins);

    free(td);

    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
uint64_t rng64(thread_data * const td)
{
    if (td->p == 8)
    {
        td->p = 0;
        blake2b_mod(td->buffer, td->buffer);
    }

    return td->buffer[td->p++];
}
//----------------------------------------------------------------------------------------------------------------------
double rng0_1(thread_data * const td)
{
    uint64_t n = rng64(td);
    n &= 0x000FFFFFFFFFFFFFULL;
    n |= 0x3FF0000000000000ULL;

    double r;
    memcpy(&r, &n, 8);

    return r - 1;
}
//----------------------------------------------------------------------------------------------------------------------
void remix_rng(thread_data * const td)
{
    char dt[64];
    char rngs[64];

    datetime(dt);

    FILE * const f = fopen("/dev/random", "r");
    if (f)
    {
        fread(rngs, 1, 64, f);
        fclose(f);
    }

    for (int i=0;i<64;i++)
    {
        unsigned char * const restrict b = (unsigned char *)td->buffer;
        b[i] ^= dt[i] ^ rngs[i];
    }

    blake2b_mod(td->buffer, td->buffer);

    td->p = 0;
}
//----------------------------------------------------------------------------------------------------------------------
void * __attribute__((noreturn)) termination_notice(void * tID)
{
    char buf[64];

    while(1)
    {
        sleep(20);

        // Check if we're getting terminated
        if (!e("curl -s --head http://169.254.169.254/latest/meta-data/spot/termination-time | head -1 | grep \"HTTP/1.[01] [23]..\" > /dev/null"))
        {
            o("%s > Termination Notice has been posted\n", datetime(buf));

            pthread_spin_lock(&cstotals);
            o("Total Samples: %lu\nTotal Evens: %lu\nFinal Ratio: %.16f\n", total_counter, total_even, total_even / (double)total_counter);
            pthread_spin_unlock(&cstotals);

            e("aws s3 cp runme.txt s3://" AWS_S3_BUCKET_NAME "/logs/term_runme_$(date +\"%m_%d_%Y_%H_%M_%S_%N\").txt");
            e("aws s3 cp output.txt s3://" AWS_S3_BUCKET_NAME "/logs/term_output_$(date +\"%m_%d_%Y_%H_%M_%S_%N\").txt");

            o("\n%s > Exiting from termination notice\n\n", datetime(buf));

            fflush(0);
            sleep(1);

            exit(0);
        }
    }

    __builtin_unreachable();
}
//----------------------------------------------------------------------------------------------------------------------