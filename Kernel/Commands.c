
#include "Commands.h"
#include "Metadata.h"
#include "PacketBuilders.h"
#include "Runner.h"
#include <File.h>
#include <Logger.h>
#include <Socket.h>
#include <stdlib.h>
#include <Timer.h>
#include <vector.h>

ScriptCommand const ScriptCommands[] =
{
    { "SELECT",   HandleSelect   },
    { "INSERT",   HandleInsert   },
    { "CREATE",   HandleCreate   },
    { "DESCRIBE", HandleDescribe },
    { "DROP",     HandleDrop     },
    { "JOURNAL",  HandleJournal  },
    { "ADD",      HandleAdd      },
    { "RUN",      HandleRun      },
    { "METRICS",  HandleMetrics  },
    { NULL,       NULL           }
};

static inline bool ValidateKey(char const* keyString, uint16_t* result)
{
    errno = 0;
    uint32_t k = strtoul(keyString, NULL, 10);
    if (errno || k > UINT16_MAX)
    {
        LISSANDRA_LOG_ERROR("Key %s invalida", keyString);
        return false;
    }

    *result = (uint16_t) k;
    return true;
}

bool HandleSelect(Vector const* args)
{
    //           cmd args
    //           0      1       2
    // sintaxis: SELECT <table> <key>

    if (Vector_size(args) != 3)
    {
        LISSANDRA_LOG_ERROR("SELECT: Uso - SELECT <tabla> <key>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];

    CriteriaType ct;
    if (!Metadata_Get(table, &ct))
    {
        LISSANDRA_LOG_ERROR("SELECT: Tabla %s no encontrada en metadata", table);
        return false;
    }

    uint16_t k;
    if (!ValidateKey(key, &k))
        return false;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Select.Key = k;

    Socket* s = Criteria_Dispatch(ct, OP_SELECT, &dbr);
    if (!s) // no hay memorias conectadas? criteria loguea el error
        return false;

    uint32_t requestTime = GetMSTime();
    Packet* p = Socket_RecvPacket(s);
    uint32_t const latency = GetMSTimeDiff(requestTime, GetMSTime());
    Criteria_AddMetric(ct, EVT_READ_LATENCY, latency);

    switch (Packet_GetOpcode(p))
    {
        case MSG_SELECT:
            break;
        case MSG_ERR_MEM_FULL:
        {
            // usar valor igual (lo devuelve) pero además mandar un Journal
            Packet* journalPacket = BuildJournal(NULL /*unused*/);
            Socket_SendPacket(s, journalPacket); // no uso el dispatcher, se envia a la memoria que me dió el error.
            Packet_Destroy(journalPacket);
            break;
        }
        case MSG_ERR_NOT_FOUND:
            LISSANDRA_LOG_ERROR("SELECT: key %hu no encontrada", k);
            Packet_Destroy(p);
            return false;
        default:
            LISSANDRA_LOG_FATAL("SELECT: recibido opcode no esperado %hu", Packet_GetOpcode(p));
            Packet_Destroy(p);
            return false;
    }

    char* value;
    Packet_Read(p, &value);

    LISSANDRA_LOG_INFO("SELECT: tabla: %s, key: %s, valor: %s", table, key, value);
    Free(value);
    Packet_Destroy(p);
    return true;
}

bool HandleInsert(Vector const* args)
{
    //           cmd args
    //           0      1       2     3
    // sintaxis: INSERT <table> <key> <value>

    // obs: INSERT desde kernel va sin Timestamp (ver #1355)
    if (Vector_size(args) != 4)
    {
        LISSANDRA_LOG_ERROR("INSERT: Uso - INSERT <table> <key> <value>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];
    char* const value = tokens[3];

    // todo: para probar
    CriteriaType ct = CRITERIA_SC;
    /*if (!Metadata_Get(table, &ct))
    {
        LISSANDRA_LOG_ERROR("INSERT: Tabla %s no encontrada en metadata", table);
        return false;
    }*/

    uint16_t k;
    if (!ValidateKey(key, &k))
        return false;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Insert.Key = k;
    dbr.Data.Insert.Value = value;

    Socket* s = Criteria_Dispatch(ct, OP_INSERT, &dbr);
    if (!s) // no hay memorias conectadas? criteria loguea el error
        return false;

    uint32_t requestTime = GetMSTime();
    Packet* p = Socket_RecvPacket(s);

    switch (Packet_GetOpcode(p))
    {
        case MSG_INSERT:
            break;
        case MSG_ERR_MEM_FULL:
        {
            // se debe vaciar la memoria e intentar el request nuevamente. Esto aumenta la latencia de escritura.
            while (Packet_GetOpcode(p) == MSG_ERR_MEM_FULL)
            {
                Packet_Destroy(p);

                Packet* journal = BuildJournal(NULL /*unused*/);
                Socket_SendPacket(s, journal);
                Packet_Destroy(journal);

                p = BuildInsert(&dbr);
                Socket_SendPacket(s, p);
                Packet_Destroy(p);

                p = Socket_RecvPacket(s);
            }
            break;
        }
        default:
            LISSANDRA_LOG_FATAL("INSERT: recibido opcode no esperado %hu", Packet_GetOpcode(p));
            Packet_Destroy(p);
            return false;
    }

    uint32_t const latency = GetMSTimeDiff(requestTime, GetMSTime());
    Criteria_AddMetric(ct, EVT_WRITE_LATENCY, latency);

    LISSANDRA_LOG_INFO("INSERT: tabla: %s, key: %s, valor: %s", table, key, value);
    Packet_Destroy(p);
    return true;
}

bool HandleCreate(Vector const* args)
{
    //           cmd args
    //           0      1       2          3            4
    // sintaxis: CREATE <table> <criteria> <partitions> <compaction_time>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("CREATE: Uso - CREATE <tabla> <criterio> <particiones> <tiempo entre compactaciones>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const criteria = tokens[2];
    char* const partitions = tokens[3];
    char* const compaction_time = tokens[4];

    if (Metadata_Get(table, NULL))
    {
        LISSANDRA_LOG_ERROR("CREATE: Tabla %s ya existe en metadata!", table);
        return false;
    }

    CriteriaType ct;
    if (!CriteriaFromString(criteria, &ct))
        return false;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Create.Consistency = ct;
    dbr.Data.Create.Partitions = strtoul(partitions, NULL, 10);
    dbr.Data.Create.CompactTime = strtoul(compaction_time, NULL, 10);

    Socket* s = Criteria_Dispatch(ct, OP_CREATE, &dbr);
    if (!s) // no hay memorias conectadas? criteria loguea el error
        return false;

    LISSANDRA_LOG_INFO("CREATE: tabla: %s, consistencia: %s, particiones: %s, tiempo compactacion: %s", table, criteria,
                       partitions, compaction_time);
    return true;
}

bool HandleDescribe(Vector const* args)
{
    //           cmd args
    //           0        1
    // sintaxis: DESCRIBE [name]

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DESCRIBE: Uso - DESCRIBE [tabla]");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
        table = tokens[1];

    DBRequest dbr;
    dbr.TableName = table;

    Metadata_Clear();
    for (CriteriaType ct = 0; ct < NUM_CRITERIA; ++ct)
    {
        Socket* s = Criteria_Dispatch(ct, OP_DESCRIBE, &dbr);
        if (!s) // no hay memorias conectadas? criteria loguea el error
            return false;

        Packet* p = Socket_RecvPacket(s);
        if (Packet_GetOpcode(p) != MSG_DESCRIBE && Packet_GetOpcode(p) != MSG_DESCRIBE_GLOBAL)
        {
            LISSANDRA_LOG_FATAL("DESCRIBE: recibido opcode no esperado %hu", Packet_GetOpcode(p));
            return false;
        }

        uint32_t num = 1;
        if (Packet_GetOpcode(p) == MSG_DESCRIBE_GLOBAL)
            Packet_Read(p, &num);

        LISSANDRA_LOG_INFO("DESCRIBE: numero de tablas %u", num);

        for (uint32_t i = 0; i < num; ++i)
        {
            char* name;
            Packet_Read(p, &name);

            uint8_t type;
            Packet_Read(p, &type);

            uint16_t partitions;
            Packet_Read(p, &partitions);

            uint32_t compaction_time;
            Packet_Read(p, &compaction_time);

            LISSANDRA_LOG_INFO(
                    "DESCRIBE: Tabla nº %u: nombre: %s, tipo consistencia %u, particiones: %u, tiempo entre compactaciones %u",
                    num + 1, name, type, partitions, compaction_time);

            Metadata_Add(name, type);
            Free(name);
        }

        Packet_Destroy(p);
    }

    return true;
}

bool HandleDrop(Vector const* args)
{
    //           cmd args
    //           0    1
    // sintaxis: DROP <name>

    if (Vector_size(args) != 2)
    {
        LISSANDRA_LOG_ERROR("DROP: Uso - DROP <tabla>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];

    CriteriaType ct;
    if (!Metadata_Get(table, &ct))
    {
        LISSANDRA_LOG_ERROR("DROP: Tabla %s no encontrada en metadata", table);
        return false;
    }

    DBRequest dbr;
    dbr.TableName = table;

    Socket* s = Criteria_Dispatch(ct, OP_DROP, &dbr);
    if (!s) // no hay memorias conectadas? criteria loguea el error
        return false;

    Metadata_Del(table);
    return true;
}

bool HandleJournal(Vector const* args)
{
    //           cmd args
    //           0
    // sintaxis: JOURNAL

    if (Vector_size(args) != 1)
    {
        LISSANDRA_LOG_ERROR("JOURNAL: Uso - JOURNAL");
        return false;
    }

    // broadcast a todos los criterios
    for (CriteriaType ct = 0; ct < NUM_CRITERIA; ++ct)
    {
        Socket* s = Criteria_Dispatch(ct, OP_JOURNAL, NULL);
        if (!s) // no hay memorias conectadas? criteria loguea el error
            return false;
    }

    return true;
}

bool HandleAdd(Vector const* args)
{
    //           cmd args
    //           0   1      2   3  4
    // sintaxis: ADD MEMORY <n> TO <criteria>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const magic = tokens[1];    // MEMORY
    char* const idx = tokens[2];      // n
    char* const magic2 = tokens[3];   // TO
    char* const criteria = tokens[4]; // criteria

    if (strcmp(magic, "MEMORY") != 0)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return false;
    }

    uint32_t memIdx = strtoul(idx, NULL, 10);
    if (!memIdx || !Criteria_MemoryExists(memIdx))
    {
        LISSANDRA_LOG_ERROR("ADD: Memoria %s no válida", idx);
        return false;
    }

    if (strcmp(magic2, "TO") != 0)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return false;
    }

    CriteriaType type;
    if (!CriteriaFromString(criteria, &type))
        return false;

    Criteria_AddMemory(type, memIdx);
    return true;
}

bool HandleRun(Vector const* args)
{
    //           cmd args
    //           0   1
    // sintaxis: RUN <path>
    if (Vector_size(args) != 2)
    {
        LISSANDRA_LOG_ERROR("RUN: Cantidad de parámetros incorrecta.");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const fileName = tokens[1];
    File* script = file_open(fileName, F_OPEN_READ);
    if (!file_is_open(script))
    {
        LISSANDRA_LOG_ERROR("RUN: Archivo no válido: %s", fileName);
        file_close(script);
        return false;
    }

    Runner_AddScript(script);
    return true;
}

bool HandleMetrics(Vector const* args)
{
    //           cmd
    //           0
    // sintaxis: METRICS
    if (Vector_size(args) != 1)
    {
        LISSANDRA_LOG_ERROR("RUN: Cantidad de parámetros incorrecta.");
        return false;
    }

    Criterias_Report();
    return true;
}
