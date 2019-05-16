
#ifndef Metric_h__
#define Metric_h__

#include <stdint.h>

typedef enum
{
    EVT_READ_LATENCY,  // loguea un tiempo de ejecucion SELECT
    EVT_WRITE_LATENCY, // loguea un tiempo de ejecucion INSERT
    EVT_MEM_READ,      // loguea un SELECT
    EVT_MEM_WRITE,     // loguea un INSERT
    EVT_MEM_OP         // loguea otra operacion
} MetricEvent;

typedef enum
{
    // por criterio
    CRITERIA_REPORTS_BEGIN,
    MEAN_READ_LATENCY = CRITERIA_REPORTS_BEGIN,  // tiempo promedio que tarda un SELECT en los ultimos 30 seg
    MEAN_WRITE_LATENCY, // tiempo promedio que tarda un INSERT en los ultimos 30 seg
    TOTAL_READS,        // cantidad de select ejecutados en los ultimos 30 seg
    TOTAL_WRITES,       // cantidad de insert ejecutados en los ultimos 30 seg

    CRITERIA_REPORTS_END,

    // por memoria
    MEMORY_REPORTS_BEGIN = CRITERIA_REPORTS_END,
    MEMORY_LOAD = MEMORY_REPORTS_BEGIN, // cantidad de INSERT/SELECT que se ejecutaron respecto de operaciones totales

    MEMORY_REPORTS_END,
    NUM_REPORTS = MEMORY_REPORTS_END
} ReportType;

typedef struct Metrics Metrics;

Metrics* Metrics_Create(void);

void Metrics_Add(Metrics*, MetricEvent, uint32_t);

void Metrics_PruneOldEvents(Metrics*);

void Metrics_Report(Metrics const*, ReportType);

void Metrics_Destroy(Metrics*);

#endif //Metric_h__
