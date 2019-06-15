
#include "Metric.h"
#include <Logger.h>
#include <Malloc.h>
#include <Timer.h>

typedef struct Metric
{
    MetricEvent Type;
    uint32_t Timestamp;

    // para los tiempos de exec
    uint32_t Value;

    struct Metric* Next;
} Metric;

typedef struct Metrics
{
    Metric* Head;
    Metric* Tail;
} Metrics;

Metrics* Metrics_Create(void)
{
    Metrics* m = Malloc(sizeof(Metrics));
    m->Head = NULL;
    m->Tail = NULL;
    return m;
}

void Metrics_Add(Metrics* m, MetricEvent event, uint32_t value)
{
    Metric* metric = Malloc(sizeof(Metric));
    metric->Type = event;
    metric->Timestamp = GetMSTime();
    metric->Value = value;
    metric->Next = NULL;

    if (!m->Head)
    {
        m->Head = metric;
        m->Tail = metric;
    }
    else
    {
        m->Tail->Next = metric;
        m->Tail = metric;
    }
}

void Metrics_PruneOldEvents(Metrics* m)
{
    // reportar eventos ocurridos en los ultimos 30 segundos inclusive
    static uint32_t const CUT_INTERVAL_MS = 30000U;

    // la lista está ordenada. el evento mas reciente es el ultimo
    // hay que considerar solo los eventos que ocurrieron hasta 30 segs antes, descartando los otros
    // por lo tanto el primer elemento que no cumpla es mi condicion de corte

    uint32_t now = GetMSTime();

    Metric* metric = m->Head;
    for (; metric != NULL; metric = metric->Next)
    {
        // busco el primer elemento mas nuevo y corto la iteracion
        if (metric->Timestamp + CUT_INTERVAL_MS >= now)
            break;
    }

    // eliminamos los elementos restantes (mas viejos que 30 seg)
    for (Metric* i = m->Head; i != metric;)
    {
        Metric* next = i->Next;
        Free(i);
        i = next;
    }
    m->Head = metric;
}

static double _meanReadLatency(Metrics const*);
static double _meanWriteLatency(Metrics const*);
static double _totalReads(Metrics const*);
static double _totalWrites(Metrics const*);
static double _memoryLoad(Metrics const*);

void Metrics_Report(Metrics const* m, ReportType report)
{
    typedef double ListIterateFn(Metrics const*);

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
    double res = itr->Iterator(m);

    LISSANDRA_LOG_INFO(itr->Format, res);
}

void Metrics_Destroy(Metrics* m)
{
    Metric* metric = m->Head;
    while (metric != NULL)
    {
        Metric* next = metric->Next;
        Free(metric);
        metric = next;
    }

    Free(m);
}

/* PRIVATE */
// horrible repeticion de codigo pero bueno, C no es C++ y se nota
static double _meanReadLatency(Metrics const* m)
{
    double res = 0.0;
    double count = 0.0;

    for (Metric* i = m->Head; i != NULL; i = i->Next)
    {
        if (i->Type != EVT_READ_LATENCY)
            continue;

        res += i->Value;
        ++count;
    }

    if (!count)
        return 0.0;
    return res / count;
}

static double _meanWriteLatency(Metrics const* m)
{
    double res = 0.0;
    double count = 0.0;

    for (Metric* i = m->Head; i != NULL; i = i->Next)
    {
        if (i->Type != EVT_WRITE_LATENCY)
            continue;

        res += i->Value;
        ++count;
    }

    if (!count)
        return 0.0;
    return res / count;
}

static double _totalReads(Metrics const* m)
{
    double count = 0.0;

    for (Metric* i = m->Head; i != NULL; i = i->Next)
    {
        if (i->Type != EVT_MEM_READ)
            continue;

        ++count;
    }

    return count;
}

static double _totalWrites(Metrics const* m)
{
    double count = 0.0;

    for (Metric* i = m->Head; i != NULL; i = i->Next)
    {
        if (i->Type != EVT_MEM_WRITE)
            continue;

        ++count;
    }

    return count;
}

static double _memoryLoad(Metrics const* m)
{
    double rwOp = 0.0;
    double total = 0.0;

    for (Metric* i = m->Head; i != NULL; i = i->Next)
    {
        switch (i->Type)
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

    if (!total)
        return 0.0;
    return rwOp / total * 100.0; // en porcentaje
}
