
#include "Handlers.h"
#include "Criteria.h"
#include "Packet.h"
#include "Socket.h"
#include <Opcodes.h>

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    { "LQL_SELECT",   0 },
    { "LQL_INSERT",   0 },
    { "LQL_CREATE",   0 },
    { "LQL_DESCRIBE", HandleDescribeOpcode }
};

void HandleDescribeOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* name;
    Packet_Read(p, &name);

    uint8_t type;
    Packet_Read(p, &type);

    uint16_t partitions;
    Packet_Read(p, &partitions);

    uint32_t compaction_time;
    Packet_Read(p, &compaction_time);

    // TODO: STUB
    // Kernel_UpdateMetadata(name, type);
}
