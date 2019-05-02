
#include "Criteria.h"
#include "Malloc.h"
#include "Opcodes.h"
#include "Packet.h"
#include "Timer.h"
#include "Socket.h"
#include <libcommons/hashmap.h>
#include <libcommons/list.h>
#include <stdlib.h>
#include <time.h>
#include <vector.h>

typedef struct
{
    MetricEvent Type;
    struct timespec Timestamp;

    // para los tiempos de exec
    uint32_t Value;
} Metric;

typedef void AddMemoryFnType(void* criteria, uint32_t memIdx);
typedef void DispatchFnType(void* criteria, MemoryOps op, DBRequest const* dbr);
typedef void DestroyFnType(void* criteria);

typedef struct
{
    t_list* Metrics;

    AddMemoryFnType* AddMemoryFn;
    DispatchFnType* DispatchFn;
    DestroyFnType* DestroyFn;
} Criteria;

typedef struct
{
    Criteria _impl;

    // SC: Maneja una sola memoria
    uint32_t MemoryIndex;
} Criteria_SC;

typedef struct
{
    Criteria _impl;

    Vector MemoryArr;
} Criteria_SHC;

typedef struct
{
    Criteria _impl;

    // EC: lista enlazada simple de memorias
    t_list* MemoryList;
} Criteria_EC;

static inline uint32_t keyHash(uint16_t key)
{
    // todo: trivial hash
    return key;
}

// map memIndex->Socket
static t_hashmap* Memories = NULL;
static Criteria* Criterias[NUM_CRITERIA] = { 0 };

static void _initBase(void* criteria, AddMemoryFnType* adder, DispatchFnType* dispatcher, DestroyFnType* destroyer);
static void _destroy(Criteria* criteria);

static AddMemoryFnType _sc_add;
static AddMemoryFnType _shc_add;
static AddMemoryFnType _ec_add;

static DispatchFnType _sc_dispatch;
static DispatchFnType _shc_dispatch;
static DispatchFnType _ec_dispatch;

static DestroyFnType _sc_destroy;
static DestroyFnType _shc_destroy;
static DestroyFnType _ec_destroy;

static void _dispatch_one(uint32_t memIndex, MemoryOps op, DBRequest const* dbr);

void Criterias_Init(void)
{
    srandom(GetMSTime());

    Memories = hashmap_create();

    // ugly pero bue
    Criteria_SC* sc = Malloc(sizeof(Criteria_SC));
    _initBase(sc, _sc_add, _sc_dispatch, _sc_destroy);
    sc->MemoryIndex = 0;

    Criteria_SHC* shc = Malloc(sizeof(Criteria_SHC));
    _initBase(shc, _shc_add, _shc_dispatch, _shc_destroy);
    Vector_Construct(&shc->MemoryArr, sizeof(uint32_t), NULL, 0);

    Criteria_EC* ec = Malloc(sizeof(Criteria_EC));
    _initBase(ec, _ec_add, _ec_dispatch, _ec_destroy);
    ec->MemoryList = list_create();

    Criterias[CRITERIA_SC] = (Criteria*) sc;
    Criterias[CRITERIA_SHC] = (Criteria*) shc;
    Criterias[CRITERIA_EC] = (Criteria*) ec;
}

void Criteria_AddMemory(CriteriaType type, uint32_t memIndex, Socket* s)
{
    Criteria* itr = Criterias[type];
    itr->AddMemoryFn(itr, memIndex);

    hashmap_put(Memories, memIndex, s);
}

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint32_t value)
{
    Metric* m = Malloc(sizeof(Metric));
    m->Type = event;
    timespec_get(&m->Timestamp, TIME_UTC);
    m->Value = value;

    list_prepend(Criterias[type]->Metrics, m);
}

void Criterias_Report(void)
{
    // TODO
}

void Criteria_Dispatch(CriteriaType type, MemoryOps op, DBRequest const* dbr)
{
    Criteria* itr = Criterias[type];
    itr->DispatchFn(itr, op, dbr);
}

void Criterias_Destroy(void)
{
    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
        _destroy(Criterias[type]);
    hashmap_destroy(Memories);
}

/* PRIVATE */
static void _initBase(void* criteria, AddMemoryFnType* adder, DispatchFnType* dispatcher, DestroyFnType* destroyer)
{
    Criteria* c = criteria;
    c->Metrics = list_create();
    c->AddMemoryFn = adder;
    c->DispatchFn = dispatcher;
    c->DestroyFn = destroyer;
}

static void _destroy(Criteria* c)
{
    list_destroy_and_destroy_elements(c->Metrics, Free);
    c->DestroyFn(c);
}

static void _sc_add(void* criteria, uint32_t memIdx)
{
    // simple reemplazo de la memoria asociada al criterio
    // PRECONDICION: no me "agregan" una memoria al SC si ya existe
    Criteria_SC* sc = criteria;
    sc->MemoryIndex = memIdx;
}

static void _shc_add(void* criteria, uint32_t memIdx)
{
    // agregar al hashmap
    // PRECONDICION: el usuario debe hacer un journal luego de agregar memorias a este criterio
    Criteria_SHC* shc = criteria;
    Vector_push_back(&shc->MemoryArr, &memIdx);
}

static void _ec_add(void* criteria, uint32_t memIdx)
{
    Criteria_EC* ec = criteria;
    list_add(ec->MemoryList, (void*) memIdx);
}

static void _sc_dispatch(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    Criteria_SC* sc = criteria;
    _dispatch_one(sc->MemoryIndex, op, dbr);
}

static void _shc_dispatch(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    Criteria_SHC* shc = criteria;

    // por defecto primer memoria (no hay nada en el tp que diga lo contrario) ver issue 1326
    size_t memPos = 0;
    switch (op)
    {
        case OP_SELECT:
            memPos = keyHash(dbr->Data.Select.Key);
            break;
        case OP_INSERT:
            memPos = keyHash(dbr->Data.Insert.Key);
            break;
        default:
            break;
    }

    memPos %= Vector_size(&shc->MemoryArr);

    uint32_t const memIdx = *((uint32_t*) Vector_at(&shc->MemoryArr, memPos));
    _dispatch_one(memIdx, op, dbr);
}

static void _ec_dispatch(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    // TODO: dejar random?
    // otras opciones:
    // LRU
    // RR
    // MR (min requests)

    Criteria_EC* ec = criteria;
    uint32_t const memIdx = (uint32_t) list_get(ec->MemoryList, random() % list_size(ec->MemoryList));
    _dispatch_one(memIdx, op, dbr);
}

static void _sc_destroy(void* criteria)
{
    // only a scalar :P
    Criteria_SC* sc = criteria;
    Free(sc);
}

static void _shc_destroy(void* criteria)
{
    Criteria_SHC* shc = criteria;
    Vector_Destruct(&shc->MemoryArr);
    Free(shc);
}

static void _ec_destroy(void* criteria)
{
    Criteria_EC* ec = criteria;
    list_destroy(ec->MemoryList);
    Free(ec);
}

static Packet* _build_select(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_SELECT, 20);
    Packet_Append(p, dbr->TableName);
    Packet_Append(p, dbr->Data.Select.Key);
    return p;
}

static Packet* _build_insert(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_INSERT, 40);
    Packet_Append(p, dbr->TableName);
    Packet_Append(p, dbr->Data.Insert.Key);
    Packet_Append(p, dbr->Data.Insert.Value);
    {
        uint32_t* ts = dbr->Data.Insert.Timestamp;
        if (ts)
            Packet_Append(p, *ts);
    }
    return p;
}

static Packet* _build_create(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_CREATE, 23);
    Packet_Append(p, dbr->TableName);
    Packet_Append(p, (uint8_t) dbr->Data.Create.Consistency);
    Packet_Append(p, dbr->Data.Create.Partitions);
    Packet_Append(p, dbr->Data.Create.CompactTime);
    return p;
}

static Packet* _build_describe(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_DESCRIBE, 16);
    {
        char const* tableName = dbr->TableName;
        if (tableName)
            Packet_Append(p, tableName);
    }
    return p;
}

static Packet* _build_drop(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_DROP, 16);
    Packet_Append(p, dbr->TableName);
    return p;
}

static Packet* _build_journal(DBRequest const* dbr)
{
    (void) dbr;
    return Packet_Create(LQL_JOURNAL, 0);
}

static void _dispatch_one(uint32_t memIndex, MemoryOps op, DBRequest const* dbr)
{
    Socket* s = hashmap_get(Memories, memIndex);

    typedef Packet* PacketBuildFn(DBRequest const*);
    static PacketBuildFn* const PacketBuilders[NUM_OPS] =
    {
        _build_select,
        _build_insert,
        _build_create,
        _build_describe,
        _build_drop,
        _build_journal
    };

    Packet* p = PacketBuilders[op](dbr);

    Socket_SendPacket(s, p);
    Packet_Destroy(p);
}
