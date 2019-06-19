
#include "PacketBuilders.h"
#include <libcommons/hashmap.h>
#include <libcommons/list.h>
#include <Logger.h>
#include <Malloc.h>
#include <pthread.h>
#include <Socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <Timer.h>
#include <vector.h>

typedef struct Memory
{
    uint32_t MemId;
    Metrics* MemMetrics;
    Socket* MemSocket;
    pthread_mutex_t MemLock;
} Memory;

typedef struct
{
    char Ip[INET6_ADDRSTRLEN];
    char Port[10 + 1]; //10 digitos, deberia ser suficiente
} MemoryConnectionData;

typedef void AddMemoryFnType(void* criteria, Memory* mem);
typedef Memory* GetMemFnType(void* criteria, MemoryOps op, DBRequest const* dbr);
typedef void ReportFnType(void const* criteria, ReportType report);
typedef void DestroyFnType(void* criteria);

typedef struct
{
    Metrics* CritMetrics;
    pthread_mutex_t CritLock;

    AddMemoryFnType* AddMemoryFn;
    GetMemFnType* GetMemFn;
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

static void _initBase(void* criteria, AddMemoryFnType* adder, GetMemFnType* getmemer, ReportFnType* reporter, DestroyFnType* destroyer);
static void _destroy_mem(void* elem);
static void _destroy_mem_array(void* pElem);
static void _destroy(Criteria* criteria);

static AddMemoryFnType _sc_add;
static AddMemoryFnType _shc_add;
static AddMemoryFnType _ec_add;

static GetMemFnType _sc_get;
static GetMemFnType _shc_get;
static GetMemFnType _ec_get;

static ReportFnType _sc_report;
static ReportFnType _shc_report;
static ReportFnType _ec_report;

static DestroyFnType _sc_destroy;
static DestroyFnType _shc_destroy;
static DestroyFnType _ec_destroy;

static void _send_one(Memory* mem, MemoryOps op, DBRequest const* dbr);
static void _report_one(Memory* mem, ReportType rt);
static void _shc_report_one(void* pElem, void* rt);
static void _ec_report_one(void* elem, void* rt);

static Criteria* Criterias[NUM_CRITERIA] = { 0 };
static t_hashmap* MemoryIPMap = NULL;

static inline MetricEvent GetEventFor(MemoryOps op)
{
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
    return evt;
}

void Criterias_Init(void)
{
    MemoryIPMap = hashmap_create();

    srandom(GetMSTime());

    // ugly pero bue
    Criteria_SC* sc = Malloc(sizeof(Criteria_SC));
    _initBase(sc, _sc_add, _sc_get, _sc_report, _sc_destroy);
    sc->SCMem = 0;

    Criteria_SHC* shc = Malloc(sizeof(Criteria_SHC));
    _initBase(shc, _shc_add, _shc_get, _shc_report, _shc_destroy);
    Vector_Construct(&shc->MemoryArr, sizeof(Memory*), _destroy_mem_array, 0);

    Criteria_EC* ec = Malloc(sizeof(Criteria_EC));
    _initBase(ec, _ec_add, _ec_get, _ec_report, _ec_destroy);
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
    pthread_mutex_init(&mem->MemLock, NULL);
    itr->AddMemoryFn(itr, mem);
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
    if (sc->SCMem && sc->SCMem->MemId == memId)
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

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint64_t value)
{
    Criteria* const itr = Criterias[type];

    pthread_mutex_lock(&itr->CritLock);
    Metrics_Add(itr->CritMetrics, event, value);
    pthread_mutex_unlock(&itr->CritLock);
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
        LISSANDRA_LOG_INFO("CRITERIO %s", CRITERIA_NAMES[type]);

        Criteria* itr = Criterias[type];
        pthread_mutex_lock(&itr->CritLock);

        Metrics_PruneOldEvents(itr->CritMetrics);

        for (ReportType r = CRITERIA_REPORTS_BEGIN; r < CRITERIA_REPORTS_END; ++r)
            Metrics_Report(itr->CritMetrics, r);

        // por ahora solo MEMORY_LOAD
        for (ReportType r = MEMORY_REPORTS_BEGIN; r < MEMORY_REPORTS_END; ++r)
            itr->ReportFn(itr, r);

        pthread_mutex_unlock(&itr->CritLock);

        LISSANDRA_LOG_INFO("\n");
    }
    LISSANDRA_LOG_INFO("======FIN REPORTE======");
}

static void ECJournalize(void* elem)
{
    Memory* const mem = elem;
    _send_one(mem, OP_JOURNAL, NULL);
}

void Criteria_BroadcastJournal(void)
{
    Criteria_SC* sc = (Criteria_SC*) Criterias[CRITERIA_SC];
    if (sc->SCMem)
        _send_one(sc->SCMem, OP_JOURNAL, NULL);

    Criteria_SHC* shc = (Criteria_SHC*) Criterias[CRITERIA_SHC];
    Memory** data = Vector_data(&shc->MemoryArr);
    for (size_t i = 0; i < Vector_size(&shc->MemoryArr); ++i)
        _send_one(data[i], OP_JOURNAL, NULL);

    Criteria_EC* ec = (Criteria_EC*) Criterias[CRITERIA_EC];
    list_iterate(ec->MemoryList, ECJournalize);
}

Memory* Criteria_GetMemoryFor(CriteriaType type, MemoryOps op, DBRequest const* dbr)
{
    // elegir cualquier criterio que tenga memorias conectadas
    if (type == CRITERIA_ANY)
    {
        CriteriaType types[NUM_CRITERIA] = { 0 };
        uint32_t last = 0;

        Criteria_SC* sc = (Criteria_SC*) Criterias[CRITERIA_SC];
        if (sc->SCMem)
            types[last++] = CRITERIA_SC;

        Criteria_SHC* shc = (Criteria_SHC*) Criterias[CRITERIA_SHC];
        if (!Vector_empty(&shc->MemoryArr))
            types[last++] = CRITERIA_SHC;

        Criteria_EC* ec = (Criteria_EC*) Criterias[CRITERIA_EC];
        if (!list_is_empty(ec->MemoryList))
            types[last++] = CRITERIA_EC;

        if (!last)
        {
            LISSANDRA_LOG_ERROR("Criteria: no hay memorias conectadas. Utilizar ADD priemero!");
            return NULL;
        }

        type = types[random() % last];
    }

    Criteria* const itr = Criterias[type];

    // registrar la operacion para metricas
    Criteria_AddMetric(type, GetEventFor(op), 0);

    return itr->GetMemFn(itr, op, dbr);
}

void Memory_SendRequest(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
    pthread_mutex_lock(&mem->MemLock);
    _send_one(mem, op, dbr);
    pthread_mutex_unlock(&mem->MemLock);
}

Packet* Memory_SendRequestWithAnswer(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
    pthread_mutex_lock(&mem->MemLock);
    _send_one(mem, op, dbr);
    Packet* p = Socket_RecvPacket(mem->MemSocket);
    pthread_mutex_unlock(&mem->MemLock);

    if (!p)
    {
        LISSANDRA_LOG_ERROR("Se desconectó memoria %u mientras leia un request!!, quitando...", mem->MemId);
        Criteria_DisconnectMemory(mem->MemId);
    }

    return p;
}

void Criterias_Destroy(void)
{
    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
        _destroy(Criterias[type]);

    hashmap_destroy_and_destroy_elements(MemoryIPMap, Free);
}

/* PRIVATE */
static void _initBase(void* criteria, AddMemoryFnType* adder, GetMemFnType* getmemer, ReportFnType* reporter, DestroyFnType* destroyer)
{
    Criteria* const c = criteria;
    c->CritMetrics = Metrics_Create();
    pthread_mutex_init(&c->CritLock, NULL);

    c->AddMemoryFn = adder;
    c->GetMemFn = getmemer;
    c->ReportFn = reporter;
    c->DestroyFn = destroyer;
}

static void _destroy_mem(void* elem)
{
    Memory* const m = elem;
    pthread_mutex_destroy(&m->MemLock);
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
    pthread_mutex_destroy(&c->CritLock);
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

static Memory* _sc_get(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    (void) dbr;

    Criteria_SC* const sc = criteria;
    if (!sc->SCMem)
    {
        LISSANDRA_LOG_ERROR("Criterio SC: sin memoria asociada! (op %d)", op);
        return NULL;
    }

    return sc->SCMem;
}

static Memory* _shc_get(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    Criteria_SHC* const shc = criteria;
    if (Vector_empty(&shc->MemoryArr))
    {
        LISSANDRA_LOG_ERROR("Criterio SHC: no hay memorias asociadas! (op %d)", op);
        return NULL;
    }

    // por defecto cualquier memoria de las que tenga el criterio (ver issue 1326)
    size_t memPos = random();
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
    return memArr[memPos];
}

static Memory* _ec_get(void* criteria, MemoryOps op, DBRequest const* dbr)
{
    (void) dbr;

    Criteria_EC* const ec = criteria;
    if (list_is_empty(ec->MemoryList))
    {
        LISSANDRA_LOG_ERROR("Criterio EC: no hay memorias asociadas! (op %d)", op);
        return NULL;
    }

    // TODO: dejar random?
    // otras opciones:
    // LRU
    // RR
    // MR (min requests)
    size_t const memPos = random() % list_size(ec->MemoryList);
    return list_get(ec->MemoryList, memPos);
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

    _report_one(mem, report);
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

static void _send_one(Memory* mem, MemoryOps op, DBRequest const* dbr)
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

    // metrica por memoria
    Metrics_Add(mem->MemMetrics, GetEventFor(op), 0);

    Packet* p = PacketBuilders[op](dbr);
    Socket_SendPacket(mem->MemSocket, p);
    Packet_Destroy(p);
}

static void _report_one(Memory* mem, ReportType rt)
{
    pthread_mutex_lock(&mem->MemLock);

    Metrics_PruneOldEvents(mem->MemMetrics);
    Metrics_Report(mem->MemMetrics, rt);

    pthread_mutex_unlock(&mem->MemLock);
}

static void _shc_report_one(void* pElem, void* rt)
{
    Memory* const* const pMem = pElem;
    Memory* const mem = *pMem;

    _report_one(mem, (ReportType) rt);
}

static void _ec_report_one(void* elem, void* rt)
{
    Memory* const mem = elem;

    _report_one(mem, (ReportType) rt);
}
