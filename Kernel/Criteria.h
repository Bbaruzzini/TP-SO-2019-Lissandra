
#ifndef Criteria_h__
#define Criteria_h__

#include "Metric.h"
#include <Logger.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct Socket Socket;

typedef enum
{
    CRITERIA_SC,
    CRITERIA_SHC,
    CRITERIA_EC,

    NUM_CRITERIA
} CriteriaType;

struct CriteriaString
{
    char const* String;
    CriteriaType Criteria;
};

static struct CriteriaString const CriteriaString[NUM_CRITERIA] =
{
    { "SC",  CRITERIA_SC },
    { "SHC", CRITERIA_SHC },
    { "EC",  CRITERIA_EC }
};

static inline bool CriteriaFromString(char const* string, CriteriaType* ct)
{
    for (uint8_t i = 0; i < NUM_CRITERIA; ++i)
    {
        if (!strcmp(string, CriteriaString[i].String))
        {
            *ct = CriteriaString[i].Criteria;
            return true;
        }
    }

    LISSANDRA_LOG_ERROR("Criterio %s no válido. Criterios validos: SC - SHC - EC.", string);
    return false;
}

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

void Criterias_Init(void);

void Criteria_ConnectMemory(uint32_t memId, char const* address, char const* serviceOrPort);

bool Criteria_MemoryExists(uint32_t memId);

void Criteria_AddMemory(CriteriaType type, uint32_t memId);

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint32_t value);

void Criterias_Report(void);

Socket* Criteria_Dispatch(CriteriaType type, MemoryOps op, DBRequest const* dbr);

void Criteria_DisconnectMemory(uint32_t memId);

void Criterias_Destroy(void);

#endif //Criteria_h__
