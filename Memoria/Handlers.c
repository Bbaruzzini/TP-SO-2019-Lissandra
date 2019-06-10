
#include "Handlers.h"
#include "API.h"
#include "MainMemory.h"
#include <Config.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    // se conecta el kernel
    { "MSG_HANDSHAKE", HandleHandshakeOpcode },

    // 5 comandos basicos
    { "LQL_SELECT",   HandleSelectOpcode },
    { "LQL_INSERT",   HandleInsertOpcode },
    { "LQL_CREATE",   HandleCreateOpcode },
    { "LQL_DESCRIBE", HandleDescribeOpcode },
    { "LQL_DROP",     HandleDropOpcode },

    // respuesta de un SELECT, no se recibe sino que se envia
    { "MSG_SELECT", NULL },

    { "MSG_INSERT", NULL },

    // respuesta del DESCRIBE, se envia
    { "MSG_DESCRIBE",        NULL },
    { "MSG_DESCRIBE_GLOBAL", NULL },

    // el kernel envia este query
    { "LQL_JOURNAL", HandleJournalOpcode },

    { "MSG_HANDSHAKE_RESPUESTA" , NULL },
    { "MSG_MEMORY_ID", NULL }
};

void HandleHandshakeOpcode(Socket* s, Packet* p)
{
    uint8_t id;
    Packet_Read(p, &id);

    //----Recibo un handshake del cliente para ver si es el kernel
    if (id != KERNEL)
    {
        LISSANDRA_LOG_ERROR("Se conecto un desconocido! (id %d)", id);
        return;
    }

    LISSANDRA_LOG_INFO("Se conecto el kernel en el fd: %d\n", s->_impl.Handle);

    uint32_t memId = config_get_long_value(sConfig, "MEMORY_NUMBER");
    Packet* resp = Packet_Create(MSG_MEMORY_ID, 4);
    Packet_Append(resp, memId);
    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);
}

void HandleSelectOpcode(Socket* s, Packet* p)
{
    char* tableName;
    uint16_t key;

    Packet_Read(p, &tableName);
    Packet_Read(p, &key);

    uint32_t maxValueLength = Memory_GetMaxValueLength();

    char* value = Malloc(maxValueLength + 1);
    API_Select(tableName, key, value);

    Packet* resp = Packet_Create(MSG_SELECT, maxValueLength + 1);
    Packet_Append(resp, value);
    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);

    Free(value);
    Free(tableName);
}

void HandleInsertOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* tableName;
    uint16_t key;
    char* value;
    bool ts;

    Packet_Read(p, &tableName);
    Packet_Read(p, &key);
    Packet_Read(p, &value);
    Packet_Read(p, &ts);

    // en el kernel el timestamp no se pone
    if (ts)
        LISSANDRA_LOG_ERROR("INSERT: Recibi un paquete de kernel con timestamp!");

    API_Insert(tableName, key, value);

    Free(tableName);
    Free(value);

    // respuesta vacia para que el kernel sepa que procese el comando
    Packet* res = Packet_Create(MSG_INSERT, 0);
    Socket_SendPacket(s, res);
    Packet_Destroy(res);
}

void HandleCreateOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* tableName;
    uint8_t ct;
    uint16_t parts;
    uint32_t compactionTime;

    Packet_Read(p, &tableName);
    Packet_Read(p, &ct);
    Packet_Read(p, &parts);
    Packet_Read(p, &compactionTime);

    API_Create(tableName, (CriteriaType) ct, parts, compactionTime);
}

static void AddToPacket(void* md, void* packet)
{
    TableMD* const p = md;
    Packet* const resp = packet;

    Packet_Append(resp, p->tableName);
    Packet_Append(resp, p->ct);
    Packet_Append(resp, p->parts);
    Packet_Append(resp, p->compTime);
}

void HandleDescribeOpcode(Socket* s, Packet* p)
{
    bool tablePresent;
    Packet_Read(p, &tablePresent);

    char* tableName = NULL;
    if (tablePresent)
        Packet_Read(p, &tableName);

    Vector v;
    Vector_Construct(&v, sizeof(TableMD), FreeMD, 0);

    API_Describe(tableName, &v);

    Packet* resp = Packet_Create(Vector_size(&v) == 1 ? MSG_DESCRIBE : MSG_DESCRIBE_GLOBAL, 100);
    if (Packet_GetOpcode(resp) == MSG_DESCRIBE_GLOBAL)
        Packet_Append(p, Vector_size(&v));

    Vector_iterate_with_data(&v, AddToPacket, resp);
    Vector_Destruct(&v);

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);

    Free(tableName);
}

void HandleDropOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* tableName;
    Packet_Read(p, &tableName);

    API_Drop(tableName);

    Free(tableName);
}

void HandleJournalOpcode(Socket* s, Packet* p)
{
    (void) s;
    (void) p;

    API_Journal();
}
