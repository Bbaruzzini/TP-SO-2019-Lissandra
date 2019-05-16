
#include "Handlers.h"
#include "Criteria.h"
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    // se conecta un cliente, no deberiamos recibirlo ya que somos clientes nosotros
    { "MSG_HANDSHAKE", NULL },

    // 5 comandos basicos comunes, el kernel los va a enviar al modulo memoria
    // por lo tanto no necesitamos manejarlos.
    { "LQL_SELECT",   NULL },
    { "LQL_INSERT",   NULL },
    { "LQL_CREATE",   NULL },
    { "LQL_DESCRIBE", NULL },
    { "LQL_DROP",     NULL },

    // respuesta de un SELECT
    { "MSG_SELECT", HandleSelectOpcode },

    // respuesta del DESCRIBE, almacenar en metadata
    { "MSG_DESCRIBE",        HandleDescribeOpcode },
    { "MSG_DESCRIBE_GLOBAL", HandleDescribeOpcode },

    // mensajes a memoria (enviado, no recibido)
    { "LQL_JOURNAL", NULL }
};

void HandleSelectOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* name;
    Packet_Read(p, &name);

    uint16_t key;
    Packet_Read(p, &key);

    char* value;
    Packet_Read(p, &value);

    uint32_t ts;
    Packet_Read(p, &ts);

    LISSANDRA_LOG_INFO("SELECT: tabla: %s, key: %" PRIu16 ", valor: %s, timestamp: %u", name, key, value, ts);

    Free(value);
    Free(name);
}

void HandleDescribeOpcode(Socket* s, Packet* p)
{
    (void) s;

    uint32_t num = 1;
    if (Packet_GetOpcode(p) == MSG_DESCRIBE_GLOBAL)
        Packet_Read(p, &num);

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
    }

    // TODO: STUB
    // Kernel_UpdateMetadata(name, type);
    // Free(name);
}
