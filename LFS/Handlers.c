
#include "Handlers.h"
#include "Lissandra.h"
#include "Packet.h"
#include "Socket.h"
#include <Logger.h>
#include <Opcodes.h>

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
    { "MSG_DESCRIBE",        NULL },
    { "MSG_DESCRIBE_GLOBAL", NULL },

    // mensaje a memoria, ignoramos
    { "LQL_JOURNAL",         NULL }
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
    ///    ///todo: mandar error? o que se hacia?
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

}

void HandleDropOpcode(Socket* s, Packet* p)
{

}
