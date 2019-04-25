
#include "Criteria.h"
#include "Malloc.h"
#include "Opcodes.h"
#include "Packet.h"
#include "Socket.h"
#include <libcommons/hashmap.h>
#include <libcommons/list.h>
#include <time.h>

typedef struct
{
    MetricEvent Type;
    struct timespec Timestamp;

    // para los tiempos de exec
    uint32_t Value;
} Metric;

typedef void AddMemoryFnType(void* criteria, uint32_t memIdx);
typedef void DispatchFnType(void* criteria, MemoryOps op);
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

    // SHC: buckets (index_tbl -> memoryIdx map)
    // TODO: convertir a un array porque es eso. el hash se aplica al elegir memoria, no al agregarla
    t_hashmap* MemoryMap;
} Criteria_SHC;

typedef struct
{
    Criteria _impl;

    // EC: lista enlazada simple de memorias
    t_list* MemoryList;
} Criteria_EC;

// map memIndex->Socket
static t_hashmap* Memories = NULL;
static Criteria* Criterias[NUM_CRITERIA] = { 0 };

static void _initBase(void* criteria, AddMemoryFnType* adder, DispatchFnType* dispatcher, DestroyFnType* destroyer);
static void _destroy(void* criteria);

static AddMemoryFnType _sc_add;
static AddMemoryFnType _shc_add;
static AddMemoryFnType _ec_add;

static DispatchFnType _sc_dispatch;
static DispatchFnType _shc_dispatch;
static DispatchFnType _ec_dispatch;

static DestroyFnType _sc_destroy;
static DestroyFnType _shc_destroy;
static DestroyFnType _ec_destroy;

static void _dispatch_one_map(int key, void* elem, void* op);
static void _dispatch_one_itr(void* elem, void* op);
static void _dispatch_one(uint32_t memIndex, MemoryOps op);

void Criterias_Init(void)
{
    Memories = hashmap_create();

    // ugly pero bue
    Criteria_SC* sc = Malloc(sizeof(Criteria_SC));
    _initBase(sc, _sc_add, _sc_dispatch, _sc_destroy);
    sc->MemoryIndex = 0;

    Criteria_SHC* shc = Malloc(sizeof(Criteria_SHC));
    _initBase(shc, _shc_add, _shc_dispatch, _shc_destroy);
    shc->MemoryMap = hashmap_create();

    Criteria_EC* ec = Malloc(sizeof(Criteria_EC));
    _initBase(ec, _ec_add, _ec_dispatch, _ec_destroy);
    ec->MemoryList = list_create();

    Criterias[CRITERIA_SC] = (Criteria*) sc;
    Criterias[CRITERIA_SHC] = (Criteria*) shc;
    Criterias[CRITERIA_SC] = (Criteria*) ec;
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

void Criteria_Dispatch(CriteriaType type, MemoryOps op)
{
    Criteria* itr = Criterias[type];
    itr->DispatchFn(itr, op);
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

static void _destroy(void* criteria)
{
    Criteria* c = criteria;

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
    hashmap_put(shc->MemoryMap, 0, (void*) memIdx);
    // TODO: en realidad no tendria que ser hashmap
}

static void _ec_add(void* criteria, uint32_t memIdx)
{
    Criteria_EC* ec = criteria;
    list_add(ec->MemoryList, (void*) memIdx);
}

static void _sc_dispatch(void* criteria, MemoryOps op)
{
    Criteria_SC* sc = criteria;
    _dispatch_one(sc->MemoryIndex, op);
}

static void _shc_dispatch(void* criteria, MemoryOps op)
{
    Criteria_SHC* shc = criteria;
    hashmap_iterate_with_data(shc->MemoryMap, _dispatch_one_map, (void*) op);
}

static void _ec_dispatch(void* criteria, MemoryOps op)
{
    Criteria_EC* ec = criteria;
    list_iterate_with_data(ec->MemoryList, _dispatch_one_itr, (void*) op);
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
    hashmap_destroy(shc->MemoryMap);
    Free(shc);
}

static void _ec_destroy(void* criteria)
{
    Criteria_EC* ec = criteria;
    list_destroy(ec->MemoryList);
    Free(ec);
}

static void _dispatch_one_map(int key, void* elem, void* op)
{
    (void) key;
    _dispatch_one((uint32_t) elem, (MemoryOps) op);
}

static void _dispatch_one_itr(void* elem, void* op)
{
    _dispatch_one((uint32_t) elem, (MemoryOps) op);
}

static void _dispatch_one(uint32_t memIndex, MemoryOps op)
{
    // TODO: stub
    Socket* s = hashmap_get(Memories, memIndex);
    Packet* p = Packet_Create(LQL_DESCRIBE, 0);

    switch (op)
    {
        case OP_CREATE:
        default:
            break;
    }

    Socket_SendPacket(s, p);
    Packet_Destroy(p);
}
