/* sysmon.c — tempo e memória (Windows usa PSAPI) */
#include "sysmon.h"
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <psapi.h>
#else
  #include <time.h>
  #include <unistd.h>
#endif

static double now_sec(void){
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
#endif
}
void timer_start(Timer *t){ t->start_sec = now_sec(); }
double timer_elapsed_sec(const Timer *t){ return now_sec() - t->start_sec; }
uint64_t mem_current_kb(void){
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if(GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return (uint64_t)(pmc.WorkingSetSize/1024);
    return 0;
#else
    FILE *f=fopen("/proc/self/statm","r"); if(!f) return 0;
    unsigned long pages=0; if(fscanf(f,"%lu",&pages)!=1){ fclose(f); return 0; } fclose(f);
    return (uint64_t)pages * (uint64_t)(sysconf(_SC_PAGESIZE)/1024);
#endif
}