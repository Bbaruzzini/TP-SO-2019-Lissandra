
#ifndef Criteria_h__
#define Criteria_h__

#include <stdint.h>

typedef struct Socket Socket;

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
    MEAN_READ_LATENCY,  // tiempo promedio que tarda un SELECT en los ultimos 30 seg
    MEAN_WRITE_LATENCY, // tiempo promedio que tarda un INSERT en los ultimos 30 seg
    TOTAL_READS,        // cantidad de select ejecutados en los ultimos 30 seg
    TOTAL_WRITES,       // cantidad de insert ejecutados en los ultimos 30 seg

    // por memoria
    MEMORY_LOAD,        // cantidad de INSERT/SELECT que se ejecutaron respecto de operaciones totales

    NUM_REPORTS
} MetricReport;

typedef enum
{
    CRITERIA_SC,
    CRITERIA_SHC,
    CRITERIA_EC,

    NUM_CRITERIA
} CriteriaType;

typedef enum
{
    OP_SELECT,
    OP_INSERT,
    OP_CREATE,
    OP_DESCRIBE,
    OP_DROP,
    OP_JOURNAL
} MemoryOps;

typedef struct
{
    char const* TableName;
    union
    {
        struct
        {
            uint16_t Key;
        } Select;

        struct
        {
            uint16_t Key;
            char const* Value;
            uint32_t* Timestamp;
        } Insert;

        struct
        {
            CriteriaType Consistency;
            uint16_t Partitions;
            uint32_t CompactTime;
        } Create;
    } Data;
} DBRequest;

void Criterias_Init(void);

void Criteria_AddMemory(CriteriaType type, uint32_t memIndex, Socket* s);

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint32_t value);

void Criterias_Report(void);

void Criteria_Dispatch(CriteriaType type, MemoryOps op, DBRequest const* dbr);

void Criterias_Destroy(void);

#endif //Criteria_h__
