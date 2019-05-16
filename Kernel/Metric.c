
#include "Metric.h"
#include <libcommons/list.h>
#include <Logger.h>
#include <Malloc.h>
#include <Timer.h>

typedef struct
{
    MetricEvent Type;
    struct timespec Timestamp;

    // para los tiempos de exec
    uint32_t Value;
} Metric;

typedef struct Metrics
{
    t_list* list;
} Metrics;

Metrics* Metrics_Create(void)
{
    Metrics* m = Malloc(sizeof(Metrics));
    m->list = list_create();
    return m;
}

void Metrics_Add(Metrics* m, MetricEvent event, uint32_t value)
{
    Metric* metric = Malloc(sizeof(Metric));

    metric->Type = event;
    timespec_get(&metric->Timestamp, TIME_UTC);
    metric->Value = value;
    list_prepend(m->list, metric);
}

void Metrics_PruneOldEvents(Metrics* m)
{
    // reportar eventos ocurridos en los ultimos 30 segundos inclusive
    static uint32_t const CUT_INTERVAL_MS = 30000U;

    struct timespec now;
    timespec_get(&now, TIME_UTC);

    // la lista está ordenada. el evento mas reciente es el primero
    // hay que considerar solo los eventos que ocurrieron hasta 30 segs antes, descartando los otros

    // necesito de alguna forma aprovechar el hecho de que la lista este ordenada
    // no puedo hacerlo con la abstraccion actual, necesito algo como splice e iteradores, asi que vamos a manopla
    size_t n = 0;
    t_link_element* p = m->list->head;
    for (; p != NULL; p = p->next)
    {
        // busco el primer elemento mas viejo y corto la iteracion
        Metric const* const metric = p->data;
        if (TimeSpecToMS(&now) < TimeSpecToMS(&metric->Timestamp) + CUT_INTERVAL_MS)
            break;

        ++n;
    }

    m->list->elements_count = n;
    m->list->tail = p;
    if (p)
    {
        // eliminamos los elementos restantes (mas viejos que 30 seg)
        for (t_link_element* i = p->next; i != NULL;)
        {
            t_link_element* next = i->next;
            Free(i->data);
            i = next;
        }
        p->next = NULL;
    }
}

static double _meanReadLatency(t_list const*);
static double _meanWriteLatency(t_list const*);
static double _totalReads(t_list const*);
static double _totalWrites(t_list const*);
static double _memoryLoad(t_list const*);

void Metrics_Report(Metrics const* m, ReportType report)
{
    typedef double ListIterateFn(t_list const*);

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
        { "Carga de memoria: %4.2f", _memoryLoad }
    };

    struct ListIterator const* const itr = ListIterators + report;
    double res = itr->Iterator(m->list);

    LISSANDRA_LOG_INFO(itr->Format, res);
}

void Metrics_Destroy(Metrics* m)
{
    list_destroy_and_destroy_elements(m->list, Free);
    Free(m);
}

/* PRIVATE */
// horrible repeticion de codigo pero bueno, C no es C++ y se nota
static double _meanReadLatency(t_list const* l)
{
    double res = 0.0;
    double count = 0.0;

    for (t_link_element* i = l->head; i != NULL; i = i->next)
    {
        Metric const* const m = i->data;
        if (m->Type != EVT_READ_LATENCY)
            continue;

        res += m->Value;
        ++count;
    }

    if (!count)
        return 0.0;
    return res / count;
}

static double _meanWriteLatency(t_list const* l)
{
    double res = 0.0;
    double count = 0.0;

    for (t_link_element* i = l->head; i != NULL; i = i->next)
    {
        Metric const* const m = i->data;
        if (m->Type != EVT_WRITE_LATENCY)
            continue;

        res += m->Value;
        ++count;
    }

    if (!count)
        return 0.0;
    return res / count;
}

static double _totalReads(t_list const* l)
{
    double count = 0.0;

    for (t_link_element* i = l->head; i != NULL; i = i->next)
    {
        Metric const* const m = i->data;
        if (m->Type != EVT_MEM_READ)
            continue;

        ++count;
    }

    return count;
}

static double _totalWrites(t_list const* l)
{
    double count = 0.0;

    for (t_link_element* i = l->head; i != NULL; i = i->next)
    {
        Metric const* const m = i->data;
        if (m->Type != EVT_MEM_WRITE)
            continue;

        ++count;
    }

    return count;
}

static double _memoryLoad(t_list const* l)
{
    double rwOp = 0.0;
    double total = 0.0;

    for (t_link_element* i = l->head; i != NULL; i = i->next)
    {
        Metric const* const m = i->data;
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

    if (!total)
        return 0.0;
    return rwOp / total;
}
