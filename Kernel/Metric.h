
#ifndef Metric_h__
#define Metric_h__

#include <stdint.h>

typedef enum
{
    EVT_READ_LATENCY,  // loguea un tiempo de ejecucion SELECT
    EVT_WRITE_LATENCY, // loguea un tiempo de ejecucion INSERT
    EVT_MEM_READ,      // loguea un SELECT
    EVT_MEM_WRITE,     // loguea un INSERT
} MetricEvent;

typedef enum
{
    MEAN_READ_LATENCY,  // tiempo promedio que tarda un SELECT en los ultimos 30 seg
    MEAN_WRITE_LATENCY, // tiempo promedio que tarda un INSERT en los ultimos 30 seg
    TOTAL_READS,        // cantidad de select ejecutados en los ultimos 30 seg
    TOTAL_WRITES,       // cantidad de insert ejecutados en los ultimos 30 seg

    NUM_REPORTS,
} ReportType;

typedef struct Metrics Metrics;

Metrics* Metrics_Create(void);

void Metrics_Add(Metrics*, MetricEvent, uint64_t);

double Metrics_PruneOldEvents(Metrics*);

void Metrics_Report(Metrics const*, ReportType);

void Metrics_Destroy(Metrics*);

#endif //Metric_h__
