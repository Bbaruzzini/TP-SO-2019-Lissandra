
#include "Handlers.h"
#include "Packet.h"
#include "Socket.h"
#include <Logger.h>
#include <Opcodes.h>

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    { "MSG_HANDSHAKE",       HandleHandshake },

    // TODO: Implementar logica funciones
    { "LQL_SELECT",          NULL            },
    { "LQL_INSERT",          NULL            },
    { "LQL_CREATE",          NULL            },
    { "LQL_DESCRIBE",        NULL            },
    { "LQL_DROP",            NULL            },

    // mensajes que nosotros enviamos, ignoramos
    { "MSG_SELECT",          NULL            },
    { "MSG_DESCRIBE",        NULL            },
    { "MSG_DESCRIBE_GLOBAL", NULL            },

    // mensaje a memoria, ignoramos
    { "LQL_JOURNAL",         NULL            }
};

void HandleHandshake(Socket* s, Packet* p)
{
    uint8_t id;
    Packet_Read(p, &id);

    //----Recibo un handshake del cliente para ver si es una memoria
    if (id != MEMORIA)
    {
        // TODO: desconectar?
        LISSANDRA_LOG_ERROR("Se conecto un desconocido! (id %d)", id);
        return;
    }

    LISSANDRA_LOG_INFO("Se conecto una memoria en el socket: %d\n", s->_impl.Handle);
}
