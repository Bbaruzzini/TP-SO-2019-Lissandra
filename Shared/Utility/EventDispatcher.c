
#include "EventDispatcher.h"
#include "FileDescInterface.h"
#include "Logger.h"
#include "Malloc.h"
#include "vector.h"
#include <assert.h>
#include <libcommons/dictionary.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define PER_LOOP_FDS 20

typedef struct EventDispatcher
{
    int Handle;

    struct epoll_event _events[PER_LOOP_FDS];
    t_dictionary* _fdiMap;
} EventDispatcher;

static EventDispatcher sDispatcher;

static struct epoll_event _genEvent(void* fdi);
static void _dispatchEvent(struct epoll_event* evt);

bool EventDispatcher_Init(void)
{
    sDispatcher.Handle = epoll_create1(0);
    if (sDispatcher.Handle < 0)
    {
        LISSANDRA_LOG_SYSERROR("epoll_create1");
        return false;
    }

    memset(sDispatcher._events, 0, sizeof(struct epoll_event) * PER_LOOP_FDS);
    sDispatcher._fdiMap = dictionary_create();
    return true;
}

void EventDispatcher_AddFDI(void* interface)
{
    FDI* fdi = interface;

    struct epoll_event evt = _genEvent(fdi);
    if (epoll_ctl(sDispatcher.Handle, EPOLL_CTL_ADD, fdi->Handle, &evt) < 0)
        LISSANDRA_LOG_SYSERROR("epoll_ctl ADD");
    dictionary_put(sDispatcher._fdiMap, fdi->Handle, fdi);
}

void EventDispatcher_Notify(void* interface)
{
    FDI* fdi = interface;

    struct epoll_event evt = _genEvent(fdi);
    if (epoll_ctl(sDispatcher.Handle, EPOLL_CTL_MOD, fdi->Handle, &evt) < 0)
        LISSANDRA_LOG_SYSERROR("epoll_ctl MOD");
}

void EventDispatcher_RemoveFDI(void* interface)
{
    FDI* fdi = interface;

    static struct epoll_event dummy;
    if (epoll_ctl(sDispatcher.Handle, EPOLL_CTL_DEL, fdi->Handle, &dummy) < 0)
        LISSANDRA_LOG_SYSERROR("epoll_ctl DEL");
    dictionary_remove_and_destroy(sDispatcher._fdiMap, fdi->Handle, FDI_Destroy);
}

void EventDispatcher_Dispatch(void)
{
    int res = epoll_wait(sDispatcher.Handle, sDispatcher._events, PER_LOOP_FDS, 0);
    if (res < 0)
    {
        if (errno != EINTR)
            LISSANDRA_LOG_SYSERROR("epoll_wait");
        return;
    }

    for (int i = 0; i < res; ++i)
        _dispatchEvent(sDispatcher._events + i);
}

void EventDispatcher_Terminate(void)
{
    dictionary_destroy_and_destroy_elements(sDispatcher._fdiMap, FDI_Destroy);
    close(sDispatcher.Handle);
}

static struct epoll_event _genEvent(void* interface)
{
    FDI* fdi = interface;

    struct epoll_event evt = { 0 };
    if (fdi->Events & EV_READ)
        evt.events |= EPOLLIN;
    if (fdi->Events & EV_WRITE)
        evt.events |= EPOLLOUT;
    evt.data.ptr = fdi;
    return evt;
}

#define INTEREST_EVENTS 2
static void _dispatchEvent(struct epoll_event* evt)
{
    FDI* fdi = evt->data.ptr;
    if (evt->events & (EPOLLERR | EPOLLHUP))
    {
        LISSANDRA_LOG_TRACE("Desconectando fd %u no válido o desconectado", fdi->Handle);
        EventDispatcher_RemoveFDI(fdi);
        return;
    }

    // chequear el valor de retorno:
    // si el callback devuelve false, tengo que quitarlo de la lista de watch de epoll
    // ya que el file descriptor se ha cerrado
    struct Event
    {
        short int CondFlag;
        FDICallbackFn* Callback;
    };

    struct Event const es[INTEREST_EVENTS] =
    {
        { EPOLLIN,  fdi->ReadCallback  },
        { EPOLLOUT, fdi->WriteCallback }
    };

    for (uint8_t i = 0; i < INTEREST_EVENTS; ++i)
    {
        if (evt->events & es[i].CondFlag)
        {
            if (es[i].Callback && !es[i].Callback(fdi))
            {
                EventDispatcher_RemoveFDI(fdi);
                return;
            }
        }
    }
}
