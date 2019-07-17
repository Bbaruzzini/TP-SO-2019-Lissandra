
#ifndef Metric_h__
#define Metric_h__

#include <stdint.h>

typedef enum
{
    EVT_MEM_OP,        // loguea operacion SELECT/INSERT en memoria
    EVT_READ_LATENCY,  // loguea un tiempo de ejecucion SELECT
    EVT_WRITE_LATENCY  // loguea un tiempo de ejecucion INSERT
} MetricEvent;

typedef struct Metrics Metrics;

Metrics* Metrics_Create(void);

void Metrics_Add(Metrics*, MetricEvent, uint64_t);

void Metrics_Report(Metrics*, uint64_t);

uint64_t Metrics_GetInsertSelect(Metrics*, uint64_t);

void Metrics_Destroy(Metrics*);

#endif //Metric_h__
