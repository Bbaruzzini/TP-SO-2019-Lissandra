
#ifndef Timer_h__
#define Timer_h__

#include "FileDescInterface.h"
#include <stdint.h>
#include <time.h>

typedef void TimerCallbackFnType(void);
typedef struct
{
    FDI _impl;

    struct timespec Interval;
    TimerCallbackFnType* Callback;
} PeriodicTimer;

// Crea un nuevo timer: intervalo en milisegundos, y funcion de llamada cuando el mismo expire
PeriodicTimer* PeriodicTimer_Create(uint32_t intervalMS, TimerCallbackFnType* callback);

// cambia el intervalo de tiempo
void PeriodicTimer_ReSetTimer(PeriodicTimer* pt, uint32_t newIntervalMS);

// destruye un periodic timer
void PeriodicTimer_Destroy(void* timer);

static inline uint32_t TimeSpecToMS(struct timespec const* ts)
{
    // nano  = 10 ^ (-9)
    // milli = 10 ^ (-3)
    // divide by 10 ^ 6 to get milli
    return ts->tv_sec * 1000U + ts->tv_nsec / 1000000U;
}

static inline struct timespec MSToTimeSpec(uint32_t ms)
{
    return (struct timespec)
    {
        .tv_sec  = ms / 1000U,
        .tv_nsec = (ms % 1000U) * 1000000U
    };
}

static inline uint32_t GetMSTimeDiff(uint32_t oldMSTime, uint32_t newMSTime)
{
    // manejamos overflows
    if (oldMSTime > newMSTime)
        return (0xFFFFFFFF - oldMSTime) + newMSTime;
    else
        return newMSTime - oldMSTime;
}

// returns current tick in milliseconds
static inline uint32_t GetMSTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return TimeSpecToMS(&ts);
}

// sleep for some ms
static inline void MSSleep(uint32_t ms)
{
    struct timespec const ts = MSToTimeSpec(ms);
    nanosleep(&ts, NULL);
}

#endif //Timer_h__
