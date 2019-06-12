
#include "API.h"
#include "FileSystemSocket.h"
#include "Frame.h"
#include "MainMemory.h"
#include <Config.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <string.h>
#include <Timer.h>

void API_Select(char const* tableName, uint16_t key, char* value)
{
    uint32_t const maxValueLength = Memory_GetMaxValueLength();

    // delay artificial acceso a memoria (read latency)
    MSSleep(config_get_long_value(sConfig, "RETARDO_MEM"));
    Frame* f = Memory_GetFrame(tableName, key);
    if (f)
    {
        *value = '\0';
        strncat(value, f->Value, maxValueLength);
        return;
    }

    // delay artificial acceso FS
    MSSleep(config_get_long_value(sConfig, "RETARDO_FS"));

    // si llegué aca es porque no encuentro el valor asi que voy a ir a buscarlo al FS
    Packet* p = Packet_Create(LQL_SELECT, 16 + 2); // adivinar tamaño
    Packet_Append(p, tableName);
    Packet_Append(p, key);
    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(FileSystemSocket);
    if (Packet_GetOpcode(p) != MSG_SELECT)
    {
        LISSANDRA_LOG_FATAL("SELECT: recibido opcode no esperado %hu", Packet_GetOpcode(p));
        return;
    }

    char* fs_value;
    Packet_Read(p, &fs_value);
    *value = '\0';
    strncat(value, fs_value, maxValueLength);
    Free(fs_value);
    Packet_Destroy(p);

    Memory_SaveNewValue(tableName, key, value);

    // delay artificial acceso a memoria (write latency)
    MSSleep(config_get_long_value(sConfig, "RETARDO_MEM"));
}

void API_Insert(char const* tableName, uint16_t key, char const* value)
{
    Memory_UpdateValue(tableName, key, value);

    // delay artificial acceso a memoria (write latency)
    MSSleep(config_get_long_value(sConfig, "RETARDO_MEM"));
}

bool API_Create(char const* tableName, CriteriaType consistency, uint16_t partitions, uint32_t compactionTime)
{
    Packet* p = Packet_Create(LQL_CREATE, 16 + 3 + 4 + 4); // adivinar tamaño
    Packet_Append(p, tableName);
    Packet_Append(p, (uint8_t) consistency);
    Packet_Append(p, partitions);
    Packet_Append(p, compactionTime);

    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    return true;
}

void API_Describe(char const* tableName, Vector* results)
{
    Packet* p = Packet_Create(LQL_DESCRIBE, 16);
    Packet_Append(p, (bool) tableName);
    if (tableName)
        Packet_Append(p, tableName);

    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    uint32_t numTables;
    p = Socket_RecvPacket(FileSystemSocket);
    switch (Packet_GetOpcode(p))
    {
        case MSG_DESCRIBE:
            numTables = 1;
            break;
        case MSG_DESCRIBE_GLOBAL:
            Packet_Read(p, &numTables);
            break;
        default:
            LISSANDRA_LOG_FATAL("DESCRIBE: recibido opcode no esperado %hu", Packet_GetOpcode(p));
            return;
    }

    Vector_reserve(results, numTables);
    for (uint32_t i = 0; i < numTables; ++i)
    {
        TableMD Metadata;
        Packet_Read(p, &Metadata.tableName);
        Packet_Read(p, &Metadata.ct);
        Packet_Read(p, &Metadata.parts);
        Packet_Read(p, &Metadata.compTime);

        Vector_push_back(results, &Metadata);
    }

    Packet_Destroy(p);
}

void API_Drop(char const* tableName)
{
    Memory_EvictPages(tableName);

    Packet* p = Packet_Create(LQL_DROP, 16);
    Packet_Append(p, tableName);
    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);
}

static void Journal_Register(void* dirtyFrame)
{
    uint32_t const maxValueLength = Memory_GetMaxValueLength();

    DirtyFrame* const df = dirtyFrame;

    Packet* p = Packet_Create(LQL_INSERT, 50);
    Packet_Append(p, df->TableName);
    Packet_Append(p, df->Key);

    char* value = Malloc(maxValueLength + 1);
    *value = '\0';
    strncat(value, df->Value, maxValueLength);
    Packet_Append(p, value);
    Free(value);

    Packet_Append(p, true); // ts present
    Packet_Append(p, df->Timestamp);

    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);
}

void API_Journal(void)
{
    Memory_DoJournal(Journal_Register);
}
