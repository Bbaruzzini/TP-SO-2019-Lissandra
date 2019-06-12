
#include "Timer.h"
#include "FDIImpl.h"
#include "Logger.h"
#include "Malloc.h"
#include <sys/timerfd.h>
#include <unistd.h>

static bool _readCb(void* periodicTimer);

PeriodicTimer* PeriodicTimer_Create(uint32_t intervalMS, TimerCallbackFnType* callback)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd < 0)
    {
        LISSANDRA_LOG_SYSERROR("timerfd_create");
        return NULL;
    }

    struct timespec const interval = MSToTimeSpec(intervalMS);
    struct itimerspec its =
    {
        .it_interval = interval,
        .it_value = interval
    };

    if (timerfd_settime(fd, 0, &its, NULL) < 0)
    {
        LISSANDRA_LOG_SYSERROR("timerfd_settime");
        return NULL;
    }

    PeriodicTimer* pt = Malloc(sizeof(PeriodicTimer));
    pt->Handle = fd;
    pt->ReadCallback = _readCb;
    pt->_destroy = PeriodicTimer_Destroy;

    pt->Interval = interval;
    pt->Callback = callback;
    return pt;
}

void PeriodicTimer_ReSetTimer(PeriodicTimer* pt, uint32_t newIntervalMS)
{
    struct timespec const newInterval = MSToTimeSpec(newIntervalMS);

    // si el nuevo intervalo es igual al que ya tengo salgo en lugar de hacer syscalls
    if (!memcmp(&pt->Interval, &newInterval, sizeof(struct timespec)))
        return;

    // disarm timer
    struct itimerspec its = { 0 };
    if (timerfd_settime(pt->Handle, 0, &its, NULL) < 0)
    {
        LISSANDRA_LOG_SYSERROR("timerfd_settime");
        return;
    }

    // rearm timer with new interval
    its.it_interval = newInterval;
    its.it_value = newInterval;
    if (timerfd_settime(pt->Handle, 0, &its, NULL) < 0)
    {
        LISSANDRA_LOG_SYSERROR("timerfd_settime");
        return;
    }

    pt->Interval = newInterval;
}

void PeriodicTimer_Destroy(void* timer)
{
    PeriodicTimer* pt = timer;
    close(pt->Handle);
    Free(pt);
}

static bool _readCb(void* periodicTimer)
{
    PeriodicTimer* pt = periodicTimer;
    uint64_t expiries;
    if (read(pt->Handle, &expiries, sizeof(uint64_t)) < 0)
    {
        // try again next time
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            LISSANDRA_LOG_SYSERROR("read");
        return true;
    }

    for (uint64_t i = 0; i < expiries; ++i)
        pt->Callback();
    return true;
}
