
#include "PacketBuilders.h"
#include <libcommons/hashmap.h>
#include <libcommons/list.h>
#include <Logger.h>
#include <Malloc.h>
#include <Socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <Timer.h>
#include <vector.h>

typedef struct
{
    uint32_t MemId;
    Metrics* MemMetrics;
    Socket* MemSocket;
} Memory;

typedef struct
{
    char Ip[INET6_ADDRSTRLEN];
    char Port[10 + 1]; //10 digitos, deberia ser suficiente
} MemoryConnectionData;

typedef void AddMemoryFnType(void* criteria, Memory* mem);
typedef Socket* DispatchFnType(void* criteria, MemoryOps op, DBRequest const* dbr);
typedef void ReportFnType(void const* criteria, ReportType report);
typedef void DestroyFnType(void* criteria);

typedef struct
{
    Metrics* CritMetrics;

    AddMemoryFnType* AddMemoryFn;
    DispatchFnType* DispatchFn;
    ReportFnType* ReportFn;
    DestroyFnType* DestroyFn;
} Criteria;

typedef struct
{
    Criteria _impl;

    // SC: Maneja una sola memoria
    Memory* SCMem;
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

static Criteria* Criterias[NUM_CRITERIA] = { 0 };

static void _initBase(void* criteria, AddMemoryFnType* adder, DispatchFnType* dispatcher, ReportFnType* reporter, DestroyFnType* destroyer);
static void _destroy_mem(void* elem);
static void _destroy_mem_array(void* pElem);
static void _destroy(Criteria* criteria);

static AddMemoryFnType _sc_add;
static AddMemoryFnType _shc_add;
static AddMemoryFnType _ec_add;

static DispatchFnType _sc_dispatch;
static DispatchFnType _shc_dispatch;
static DispatchFnType _ec_dispatch;

static ReportFnType _sc_report;
static ReportFnType _shc_report;
static ReportFnType _ec_report;

static DestroyFnType _sc_destroy;
static DestroyFnType _shc_destroy;
static DestroyFnType _ec_destroy;

static Socket* _dispatch_one(Memory* mem, MemoryOps op, DBRequest const* dbr);
static void _shc_report_one(void* pElem, void* rt);
static void _ec_report_one(void* elem, void* rt);

static t_hashmap* MemoryIPMap = NULL;

void Criterias_Init(void)
{
    MemoryIPMap = hashmap_create();

    srandom(GetMSTime());

    // ugly pero bue
    Criteria_SC* sc = Malloc(sizeof(Criteria_SC));
    _initBase(sc, _sc_add, _sc_dispatch, _sc_report, _sc_destroy);
    sc->SCMem = 0;

    Criteria_SHC* shc = Malloc(sizeof(Criteria_SHC));
    _initBase(shc, _shc_add, _shc_dispatch, _shc_report, _shc_destroy);
    Vector_Construct(&shc->MemoryArr, sizeof(Memory*), _destroy_mem_array, 0);

    Criteria_EC* ec = Malloc(sizeof(Criteria_EC));
    _initBase(ec, _ec_add, _ec_dispatch, _ec_report, _ec_destroy);
    ec->MemoryList = list_create();

    Criterias[CRITERIA_SC] = (Criteria*) sc;
    Criterias[CRITERIA_SHC] = (Criteria*) shc;
    Criterias[CRITERIA_EC] = (Criteria*) ec;
}

void Criteria_ConnectMemory(uint32_t memId, char const* address, char const* serviceOrPort)
{
    if (Criteria_MemoryExists(memId))
    {
        LISSANDRA_LOG_ERROR("Intento de agregar memoria %u ya existente!", memId);
        return;
    }

    MemoryConnectionData* mcd = Malloc(sizeof(MemoryConnectionData));
    snprintf(mcd->Ip, INET6_ADDRSTRLEN, "%s", address);
    snprintf(mcd->Port, 10 + 1, "%s", serviceOrPort);

    hashmap_put(MemoryIPMap, memId, mcd);
}

bool Criteria_MemoryExists(uint32_t memId)
{
    return hashmap_has_key(MemoryIPMap, memId);
}

void Criteria_AddMemory(CriteriaType type, uint32_t memId)
{
    Criteria* const itr = Criterias[type];

    MemoryConnectionData* mcd = hashmap_get(MemoryIPMap, memId);
    if (!mcd)
    {
        LISSANDRA_LOG_ERROR("Criteria_AddMemory: Intento de agregar memoria %u no existente", memId);
        return;
    }

    SocketOpts const so =
    {
        .HostName = mcd->Ip,
        .ServiceOrPort = mcd->Port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    Socket* s = Socket_Create(&so);
    if (!s)
    {
        LISSANDRA_LOG_ERROR("Criteria_AddMemory: Error al crear socket para memoria %u!", memId);
        return;
    }

    Memory* mem = Malloc(sizeof(Memory));
    mem->MemId = memId;
    mem->MemMetrics = Metrics_Create();
    mem->MemSocket = s;
    itr->AddMemoryFn(itr, mem);
}

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint32_t value)
{
    Criteria* const itr = Criterias[type];
    Metrics_Add(itr->CritMetrics, event, value);
}

void Criterias_Report(void)
{
    static char const* const CRITERIA_NAMES[NUM_CRITERIA] =
    {
        "STRONG",
        "STRONG HASH",
        "EVENTUAL"
    };

    LISSANDRA_LOG_INFO("========REPORTE========");
    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
    {
        Criteria const* itr = Criterias[type];
        Metrics_PruneOldEvents(itr->CritMetrics);

        LISSANDRA_LOG_INFO("CRITERIO %s", CRITERIA_NAMES[type]);
        for (ReportType r = CRITERIA_REPORTS_BEGIN; r < CRITERIA_REPORTS_END; ++r)
            Metrics_Report(itr->CritMetrics, r);

        // por ahora solo MEMORY_LOAD
        for (ReportType r = MEMORY_REPORTS_BEGIN; r < MEMORY_REPORTS_END; ++r)
            itr->ReportFn(itr, r);

        LISSANDRA_LOG_INFO("\n");
    }
    LISSANDRA_LOG_INFO("======FIN REPORTE======");
}

Socket* Criteria_Dispatch(CriteriaType type, MemoryOps op, DBRequest const* dbr)
{
    Criteria* const itr = Criterias[type];

    MetricEvent evt;
    switch (op)
    {
        case OP_SELECT:
            evt = EVT_MEM_READ;
            break;
        case OP_INSERT:
            evt = EVT_MEM_WRITE;
            break;
        default:
            evt = EVT_MEM_OP;
            break;
    }

    // registrar la operacion para metricas
    Criteria_AddMetric(type, evt, 0);
    return itr->DispatchFn(itr, op, dbr);
}

static bool FindMemByIdPred(void* mem, void* id)
{
    Memory* const m = mem;
    return m->MemId == (uint32_t) id;
}

void Criteria_DisconnectMemory(uint32_t memId)
{
    if (!Criteria_MemoryExists(memId))
    {
        LISSANDRA_LOG_ERROR("Intento de quitar memoria %u no existente!", memId);
        return;
    }

    Criteria_SC* sc = (Criteria_SC*) Criterias[CRITERIA_SC];
    if (sc->SCMem->MemId == memId)
    {
        _destroy_mem(sc->SCMem);
        sc->SCMem = NULL;
    }

    Criteria_SHC* shc = (Criteria_SHC*) Criterias[CRITERIA_SHC];
    Memory** data = Vector_data(&shc->MemoryArr);
    for (size_t i = 0; i < Vector_size(&shc->MemoryArr); ++i)
    {
        if (data[i]->MemId == memId)
        {
            Vector_erase(&shc->MemoryArr, i);
            break;
        }
    }

    Criteria_EC* ec = (Criteria_EC*) Criterias[CRITERIA_EC];
    list_remove_and_destroy_by_condition(ec->MemoryList, FindMemByIdPred, (void*) memId, _destroy_mem);

    hashmap_remove_and_destroy(MemoryIPMap, memId, Free);
}

void Criterias_Destroy(void)
{
    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
        _destroy(Criterias[type]);

    hashmap_destroy_and_destroy_elements(MemoryIPMap, Free);
}

/* PRIVATE */
static void _initBase(void* criteria, AddMemoryFnType* adder, DispatchFnType* dispatcher, ReportFnType* reporter, DestroyFnType* destroyer)
{
    Criteria* const c = criteria;
    c->CritMetrics = Metrics_Create();
    c->AddMemoryFn = adder;
    c->DispatchFn = dispatcher;
    c->ReportFn = reporter;
    c->DestroyFn = destroyer;
}

static void _destroy_mem(void* elem)
{
    Memory* const m = elem;
    Metrics_Destroy(m->MemMetrics);
    Socket_Destroy(m->MemSocket);
    Free(m);
}

static void _destroy_mem_array(void* pElem)
{
    Memory* const* const pMem = pElem;
    Memory* const mem = *pMem;
    _destroy_mem(mem);
}

static void _destroy(Criteria* c)
{
    Metrics_Destroy(c->CritMetrics);
    c->DestroyFn(c);
}

static void _sc_add(void* criteria, Memory* mem)
{
    // simple reemplazo de la memoria asociada al criterio
    // PRECONDICION: no me "agregan" una memoria al SC si ya existe
    Criteria_SC* const sc = criteria;
    sc->SCMem = mem;
}

static void _shc_add(void* criteria, Memory* mem)
{
    // agregar al hashmap
    // PRECONDICION: el usuario debe hacer un journal luego de agregar memorias a este criterio
    Criteria_SHC* const shc = criteria;
    Vector_push_back(&shc->MemoryArr, &mem);
}

static void _ec_add(void* criteria, Memory* mem)
{
    Criteria_EC* const ec = criteria;
    list_add(ec->MemoryList, mem);
}

static Socket* _sc_dispatch(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    Criteria_SC* const sc = criteria;
    if (!sc->SCMem)
    {
        LISSANDRA_LOG_ERROR("Criterio SC: sin memoria asociada! (op %d)", op);
        return NULL;
    }

    return _dispatch_one(sc->SCMem, op, dbr);
}

static Socket* _shc_dispatch(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    Criteria_SHC* const shc = criteria;
    if (Vector_empty(&shc->MemoryArr))
    {
        LISSANDRA_LOG_ERROR("Criterio SHC: no hay memorias asociadas! (op %d)", op);
        return NULL;
    }

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

    Memory* const* const memArr = Vector_data(&shc->MemoryArr);
    Memory* const mem = memArr[memPos];
    return _dispatch_one(mem, op, dbr);
}

static Socket* _ec_dispatch(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    // TODO: dejar random?
    // otras opciones:
    // LRU
    // RR
    // MR (min requests)

    Criteria_EC* const ec = criteria;
    if (list_is_empty(ec->MemoryList))
    {
        LISSANDRA_LOG_ERROR("Criterio EC: no hay memorias asociadas! (op %d)", op);
        return NULL;
    }

    Memory* const mem = list_get(ec->MemoryList, random() % list_size(ec->MemoryList));
    return _dispatch_one(mem, op, dbr);
}

static void _sc_report(void const* criteria, ReportType report)
{
    Criteria_SC const* const sc = criteria;
    Memory* const mem = sc->SCMem;
    if (!mem)
    {
        LISSANDRA_LOG_ERROR("SC: memoria no asociada!");
        return;
    }

    Metrics_PruneOldEvents(mem->MemMetrics);
    Metrics_Report(mem->MemMetrics, report);
}

static void _shc_report(void const* criteria, ReportType report)
{
    Criteria_SHC const* const shc = criteria;
    Vector_iterate_with_data(&shc->MemoryArr, _shc_report_one, (void*) report);
}

static void _ec_report(void const* criteria, ReportType report)
{
    Criteria_EC const* const ec = criteria;
    list_iterate_with_data(ec->MemoryList, _ec_report_one, (void*) report);
}

static void _sc_destroy(void* criteria)
{
    Criteria_SC* const sc = criteria;
    if (sc->SCMem)
        _destroy_mem(sc->SCMem);
    Free(sc);
}

static void _shc_destroy(void* criteria)
{
    Criteria_SHC* const shc = criteria;
    Vector_Destruct(&shc->MemoryArr);
    Free(shc);
}

static void _ec_destroy(void* criteria)
{
    Criteria_EC* const ec = criteria;
    list_destroy_and_destroy_elements(ec->MemoryList, _destroy_mem);
    Free(ec);
}

static Socket* _dispatch_one(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
    typedef Packet* PacketBuildFn(DBRequest const*);
    static PacketBuildFn* const PacketBuilders[NUM_OPS] =
    {
        BuildSelect,
        BuildInsert,
        BuildCreate,
        BuildDescribe,
        BuildDrop,
        BuildJournal
    };

    Packet* p = PacketBuilders[op](dbr);

    Socket_SendPacket(mem->MemSocket, p);
    Packet_Destroy(p);
    return mem->MemSocket;
}

static void _shc_report_one(void* pElem, void* rt)
{
    Memory* const* const pMem = pElem;
    Memory* const mem = *pMem;

    Metrics_PruneOldEvents(mem->MemMetrics);
    Metrics_Report(mem->MemMetrics, (ReportType) rt);
}

static void _ec_report_one(void* elem, void* rt)
{
    Memory* const mem = elem;

    Metrics_PruneOldEvents(mem->MemMetrics);
    Metrics_Report(mem->MemMetrics, (ReportType) rt);
}
