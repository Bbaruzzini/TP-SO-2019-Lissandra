
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

typedef struct Metrics Metrics;

Metrics* Metrics_Create(void);

void Metrics_Add(Metrics*, MetricEvent, uint64_t);

uint64_t Metrics_Report(Metrics*);

uint64_t Metrics_GetInsertSelect(Metrics*);

void Metrics_Destroy(Metrics*);

#endif //Metric_h__
