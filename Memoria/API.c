
#include "API.h"
#include "FileSystemSocket.h"
#include "Frame.h"
#include "MainMemory.h"
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <string.h>

void API_Select(char const* tableName, uint16_t key, char* value)
{
    uint32_t const maxValueLength = Memory_GetMaxValueLength();

    Frame* f = Memory_GetFrame(tableName, key);
    if (f)
    {
        *value = '\0';
        strncat(value, f->Value, maxValueLength);
        return;
    }

    // si llegué aca es porque no encuentro el valor asi que voy a ir a buscarlo al FS
    Packet* p = Packet_Create(LQL_SELECT, 18);
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
}

void API_Insert(char const* tableName, uint16_t key, char const* value)
{
    Memory_UpdateValue(tableName, key, value);
}

bool API_Create(char const* tableName, char const* consistency, uint32_t partitions, uint32_t compactionTime)
{
    return true;
}

void API_Describe(char const* tableName, Vector* results)
{

}

void API_Drop(char const* tableName)
{

}

void API_Journal(void)
{

}
