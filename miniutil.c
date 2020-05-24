#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//----------------------------------------------------------------------------------------------------------------------
#include "miniutil.h"
//----------------------------------------------------------------------------------------------------------------------
char * datetime(char * const restrict buf)
{
    struct tm l;
    time_t t = time(0);
    localtime_r(&t, &l);
    asctime_r(&l, buf);
    buf[strlen(buf) - 1] = 0;
    return buf;
}
//----------------------------------------------------------------------------------------------------------------------
uint64_t tick(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000ULL) + (now.tv_nsec / 1000000ULL);
}
//----------------------------------------------------------------------------------------------------------------------
void o(char const * const restrict format, ... )
{
    extern pthread_spinlock_t csoutput;
    pthread_spin_lock(&csoutput);
    va_list t;
    va_start(t, format);
    vprintf(format, t);
    va_end(t);
    FILE * const restrict f = fopen("output.txt", "a");
    va_start(t, format);
    vfprintf(f, format, t);
    va_end(t);
    fclose(f);
    fflush(0);
    pthread_spin_unlock(&csoutput);
}
//----------------------------------------------------------------------------------------------------------------------
int e(char const * const restrict command)
{
    extern pthread_spinlock_t csexec;
    pthread_spin_lock(&csexec);
    int ret = system(command);
    if (WIFEXITED(ret)) ret = WEXITSTATUS(ret);
    else ret = -1;
    pthread_spin_unlock(&csexec);
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------