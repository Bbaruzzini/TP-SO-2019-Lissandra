
#include "Handlers.h"
#include "Kernel/Criteria.h"
#include "Lissandra.h"
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    { "MSG_HANDSHAKE",       NULL },

    { "LQL_SELECT",          HandleSelectOpcode },
    { "LQL_INSERT",          HandleInsertOpcode },
    { "LQL_CREATE",          HandleCreateOpcode },
    { "LQL_DESCRIBE",        HandleDescribeOpcode },
    { "LQL_DROP",            HandleDropOpcode },

    // mensajes que nosotros enviamos, ignoramos
    { "MSG_SELECT",          NULL },
    { "MSG_INSERT",          NULL },
    { "MSG_DESCRIBE",        NULL },
    { "MSG_DESCRIBE_GLOBAL", NULL },

    // mensaje a memoria, ignoramos
    { "LQL_JOURNAL",         NULL },

    { "MSG_HANDSHAKE_RESPUESTA", NULL },
    { "MSG_MEMORY_ID",           NULL },

    { "MSG_ERR_NOT_FOUND",        NULL },
    { "MSG_ERR_MEM_FULL",         NULL },
    { "MSG_ERR_TABLE_NOT_EXISTS", NULL }
};

/// TODO: Implementar logica funciones
void HandleSelectOpcode(Socket* s, Packet* p)
{
    /* char*: nombre tabla
    * uint16: key
    *
    * Responde: MSG_SELECT
    */

    char* nombreTabla;
    uint16_t key;
    Packet_Read(p, &nombreTabla);
    Packet_Read(p, &key);

    ///una respuesta al paquete bien podria ser de este estilo
    char* valor = strdup("SELECT NO IMPLEMENTADO");
    ///if (!lfs_select(nombreTabla, key, &valor))
    ///{
    ///    LISSANDRA_LOG_ERROR("Recibi tabla/key (%s/%d) invalida!", nombreTabla, key);
    ///    ///todo: manejo de errores
    //////////todo: MSG_ERR_NOT_FOUND
    ///    return;
    ///}

    Packet* respuesta = Packet_Create(MSG_SELECT, confLFS->TAMANIO_VALUE);
    Packet_Append(respuesta, valor);
    Socket_SendPacket(s, respuesta);
    Packet_Destroy(respuesta);

    Free(valor);

    Free(nombreTabla);
    ///Packet_Destroy no es necesario aca porque lo hace la funcion atender_memoria!
}

void HandleInsertOpcode(Socket* s, Packet* p)
{

}

void HandleCreateOpcode(Socket* s, Packet* p)
{

}

void HandleDescribeOpcode(Socket* s, Packet* p)
{
    // dummy asi puedo probar!
    Packet* resp = Packet_Create(MSG_DESCRIBE, 50);
    Packet_Append(resp, "PROBANDO");
    Packet_Append(resp, (uint8_t) CRITERIA_SC);
    Packet_Append(resp, (uint16_t) 1);
    Packet_Append(resp, (uint32_t) 30000);

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);
}

void HandleDropOpcode(Socket* s, Packet* p)
{

}
