/* sysmon.h — medição simples de tempo e memória */
#ifndef SYSMON_H
#define SYSMON_H
#include <stdint.h>
typedef struct
{
    double start_sec;
} Timer;
void timer_start(Timer *t);
double timer_elapsed_sec(const Timer *t);
uint64_t mem_current_kb(void);
#endif /* SYSMON_H */