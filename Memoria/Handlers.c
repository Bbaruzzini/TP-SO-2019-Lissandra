
#include "Handlers.h"
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

    // respuesta del DESCRIBE, se envia
    { "MSG_DESCRIBE",        NULL },
    { "MSG_DESCRIBE_GLOBAL", NULL },

    // el kernel envia este query
    { "LQL_JOURNAL", HandleJournalOpcode }
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
}

void HandleSelectOpcode(Socket* s, Packet* p)
{

}

void HandleInsertOpcode(Socket* s, Packet* p)
{

}

void HandleCreateOpcode(Socket* s, Packet* p)
{

}

void HandleDescribeOpcode(Socket* s, Packet* p)
{

}

void HandleDropOpcode(Socket* s, Packet* p)
{

}

void HandleJournalOpcode(Socket* s, Packet* p)
{

}
