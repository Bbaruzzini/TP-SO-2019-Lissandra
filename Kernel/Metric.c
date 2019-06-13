
#include "Metric.h"
#include <LockedQueue.h>
#include <Logger.h>
#include <Malloc.h>
#include <Timer.h>

typedef struct
{
    MetricEvent Type;
    uint32_t Timestamp;

    // para los tiempos de exec
    uint32_t Value;
} Metric;

typedef struct Metrics
{
    LockedQueue* list;
} Metrics;

Metrics* Metrics_Create(void)
{
    Metrics* m = Malloc(sizeof(Metrics));
    m->list = LockedQueue_Create();
    return m;
}

void Metrics_Add(Metrics* m, MetricEvent event, uint32_t value)
{
    Metric* metric = Malloc(sizeof(Metric));

    metric->Type = event;
    metric->Timestamp = GetMSTime();
    metric->Value = value;
    LockedQueue_Add(m->list, metric);
}

void Metrics_PruneOldEvents(Metrics* m)
{
    // reportar eventos ocurridos en los ultimos 30 segundos inclusive
    static uint32_t const CUT_INTERVAL_MS = 30000U;

    // la lista está ordenada. el evento mas reciente es el ultimo
    // hay que considerar solo los eventos que ocurrieron hasta 30 segs antes, descartando los otros

    // necesito de alguna forma aprovechar el hecho de que la lista este ordenada
    // no puedo hacerlo con la abstraccion actual, necesito algo como splice e iteradores, asi que vamos a manopla
    pthread_mutex_lock(&m->list->_lock);

    uint32_t now = GetMSTime();

    size_t n = 0;
    Node* p = m->list->_head;
    for (; p != NULL; p = p->Next)
    {
        // busco el primer elemento mas nuevo y corto la iteracion
        Metric const* const metric = p->Data;
        if (metric->Timestamp + CUT_INTERVAL_MS >= now)
            break;

        ++n;
    }

    // eliminamos los elementos restantes (mas viejos que 30 seg)
    for (Node* i = m->list->_head; i != p;)
    {
        Node* next = i->Next;
        Free(i->Data);
        i = next;
    }
    m->list->_head = p;

    pthread_mutex_unlock(&m->list->_lock);
}

static double _meanReadLatency(LockedQueue*);
static double _meanWriteLatency(LockedQueue*);
static double _totalReads(LockedQueue*);
static double _totalWrites(LockedQueue*);
static double _memoryLoad(LockedQueue*);

void Metrics_Report(Metrics const* m, ReportType report)
{
    typedef double ListIterateFn(LockedQueue*);

    struct ListIterator
    {
        char const* Format;
        ListIterateFn* Iterator;
    };

    static struct ListIterator const ListIterators[NUM_REPORTS] =
    {
        { "Latencia promedio SELECT/30s: %.0fms", _meanReadLatency },
        { "Latencia promedio INSERT/30s: %.0fms", _meanWriteLatency },
        { "Cantidad SELECT/30s: %.0f", _totalReads },
        { "Cantidad INSERT/30s: %.0f", _totalWrites },
        { "Carga de memoria: %4.2f%%", _memoryLoad }
    };

    struct ListIterator const* const itr = ListIterators + report;
    double res = itr->Iterator(m->list);

    LISSANDRA_LOG_INFO(itr->Format, res);
}

void Metrics_Destroy(Metrics* m)
{
    LockedQueue_Destroy(m->list, Free);
    Free(m);
}

/* PRIVATE */
// horrible repeticion de codigo pero bueno, C no es C++ y se nota
static double _meanReadLatency(LockedQueue* l)
{
    double res = 0.0;
    double count = 0.0;

    pthread_mutex_lock(&l->_lock);
    for (Node* i = l->_head; i != NULL; i = i->Next)
    {
        Metric const* const m = i->Data;
        if (m->Type != EVT_READ_LATENCY)
            continue;

        res += m->Value;
        ++count;
    }
    pthread_mutex_unlock(&l->_lock);

    if (!count)
        return 0.0;
    return res / count;
}

static double _meanWriteLatency(LockedQueue* l)
{
    double res = 0.0;
    double count = 0.0;

    pthread_mutex_lock(&l->_lock);
    for (Node* i = l->_head; i != NULL; i = i->Next)
    {
        Metric const* const m = i->Data;
        if (m->Type != EVT_WRITE_LATENCY)
            continue;

        res += m->Value;
        ++count;
    }
    pthread_mutex_unlock(&l->_lock);

    if (!count)
        return 0.0;
    return res / count;
}

static double _totalReads(LockedQueue* l)
{
    double count = 0.0;

    pthread_mutex_lock(&l->_lock);
    for (Node* i = l->_head; i != NULL; i = i->Next)
    {
        Metric const* const m = i->Data;
        if (m->Type != EVT_MEM_READ)
            continue;

        ++count;
    }
    pthread_mutex_unlock(&l->_lock);

    return count;
}

static double _totalWrites(LockedQueue* l)
{
    double count = 0.0;

    pthread_mutex_lock(&l->_lock);
    for (Node* i = l->_head; i != NULL; i = i->Next)
    {
        Metric const* const m = i->Data;
        if (m->Type != EVT_MEM_WRITE)
            continue;

        ++count;
    }
    pthread_mutex_unlock(&l->_lock);

    return count;
}

static double _memoryLoad(LockedQueue* l)
{
    double rwOp = 0.0;
    double total = 0.0;

    pthread_mutex_lock(&l->_lock);
    for (Node* i = l->_head; i != NULL; i = i->Next)
    {
        Metric const* const m = i->Data;
        switch (m->Type)
        {
            case EVT_MEM_WRITE:
            case EVT_MEM_READ:
                ++rwOp;
                //no break
            case EVT_MEM_OP:
                ++total;
                break;
            default:
                continue;
        }
    }
    pthread_mutex_unlock(&l->_lock);

    if (!total)
        return 0.0;
    return rwOp / total * 100.0; // en porcentaje
}
