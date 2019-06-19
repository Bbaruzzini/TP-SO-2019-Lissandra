
#ifndef Criteria_h__
#define Criteria_h__

#include "Metric.h"
#include <Consistency.h>

typedef struct Memory Memory;
typedef struct Packet Packet;
typedef struct Socket Socket;

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

void Criteria_DisconnectMemory(uint32_t memId);

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint64_t value);

void Criterias_Report(void);

void Criteria_BroadcastJournal(void);

Memory* Criteria_GetMemoryFor(CriteriaType type, MemoryOps op, DBRequest const* dbr);

void Memory_SendRequest(Memory* mem, MemoryOps op, DBRequest const* dbr);

Packet* Memory_SendRequestWithAnswer(Memory* mem, MemoryOps op, DBRequest const* dbr);

void Criterias_Destroy(void);

#endif //Criteria_h__
