
#ifndef Criteria_h__
#define Criteria_h__

#include "Metric.h"
#include <stdint.h>

typedef struct Socket Socket;

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
    OP_JOURNAL,

    NUM_OPS
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
        } Insert;

        struct
        {
            CriteriaType Consistency;
            uint16_t Partitions;
            uint32_t CompactTime;
        } Create;
    } Data;
} DBRequest;

bool CriteriaFromString(char const* string, CriteriaType* ct);

void Criterias_Init(void);

void Criteria_AddMemory(CriteriaType type, Socket* s);

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint32_t value);

void Criterias_Report(void);

Socket* Criteria_Dispatch(CriteriaType type, MemoryOps op, DBRequest const* dbr);

void Criterias_Destroy(void);

#endif //Criteria_h__
