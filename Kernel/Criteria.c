
#include "Criteria.h"
#include <libcommons/hashmap.h>
#include <libcommons/list.h>
#include <Logger.h>
#include <Malloc.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <stdlib.h>
#include <Timer.h>
#include <vector.h>

typedef struct
{
    Metrics* MemMetrics;
    Socket* MemSocket;
} Memory;

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

bool CriteriaFromString(char const* string, CriteriaType* ct)
{
    struct CriteriaString
    {
        char const* String;
        CriteriaType Criteria;
    };

    static struct CriteriaString const cs[NUM_CRITERIA] =
    {
        { "SC",  CRITERIA_SC },
        { "SHC", CRITERIA_SHC },
        { "EC",  CRITERIA_EC }
    };

    for (uint8_t i = 0; i < NUM_CRITERIA; ++i)
    {
        if (!strcmp(string, cs[i].String))
        {
            *ct = cs[i].Criteria;
            return true;
        }
    }

    LISSANDRA_LOG_ERROR("Criterio %s no válido. Criterios validos: SC - SHC - EC.", string);
    return false;
}

void Criterias_Init(void)
{
    srandom(GetMSTime());

    // ugly pero bue
    Criteria_SC* sc = Malloc(sizeof(Criteria_SC));
    _initBase(sc, _sc_add, _sc_dispatch, _sc_report, _sc_destroy);
    sc->SCMem = NULL;

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

void Criteria_AddMemory(CriteriaType type, Socket* s)
{
    Criteria* const itr = Criterias[type];

    Memory* mem = Malloc(sizeof(Memory));
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

void Criterias_Destroy(void)
{
    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
        _destroy(Criterias[type]);
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
    // EventDispatcher elimina el socket
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
    Packet_Append(p, (bool) false);
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
        Packet_Append(p, (bool) tableName);
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

static Socket* _dispatch_one(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
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

    Socket* const s = mem->MemSocket;
    Packet* p = PacketBuilders[op](dbr);

    Socket_SendPacket(s, p);
    Packet_Destroy(p);
    return s;
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
