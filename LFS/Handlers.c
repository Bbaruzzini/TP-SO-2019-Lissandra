
#include "Handlers.h"
#include "Lissandra.h"
#include <Consistency.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <Timer.h>

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
    char* nombreTabla;
    uint16_t key;
    char* value;
    uint32_t timestamp = GetMSEpoch();
    Packet_Read(p, &nombreTabla);
    Packet_Read(p, &key);
    Packet_Read(p, &value);

    bool ts_present;
    Packet_Read(p, &ts_present);

    if (ts_present)
        Packet_Read(p, &timestamp);

    //Hay que agregar algo al define de Packet_Read?????
    //Packet_Read(p, &timestamp);

    //resultadoInsert es EXIT_SUCCESS o EXIT_FAILURE
    int resultadoInsert = insert(nombreTabla, key, value, (time_t) timestamp);
    if (resultadoInsert == EXIT_FAILURE)
    {
        LISSANDRA_LOG_ERROR("No se pudo realizar el insert de: %s", nombreTabla);
    }

    //No estoy segura si va MSG_INSERT u otra cosa :/
    Packet* respuesta = Packet_Create(MSG_INSERT_RESPUESTA, 2);
    Packet_Append(respuesta, resultadoInsert);
    Socket_SendPacket(s, respuesta);
    Packet_Destroy(respuesta);

    free(value);
    free(nombreTabla);

}

void HandleCreateOpcode(Socket* s, Packet* p)
{
    char* nombreTabla;
    uint8_t tipoConsistencia;
    uint16_t numeroParticiones;
    uint32_t compactionTime;

    Packet_Read(p, &nombreTabla);
    Packet_Read(p, &tipoConsistencia);
    Packet_Read(p, &numeroParticiones);
    Packet_Read(p, &compactionTime);

    int resultadoCreate = create(nombreTabla, tipoConsistencia, numeroParticiones, compactionTime);

    if (resultadoCreate == EXIT_FAILURE)
    {
        LISSANDRA_LOG_ERROR("No se pudo crear la tabla: %s", nombreTabla);
    }

    //Hay que crear un MSG_CREATE?????
    Packet* respuesta = Packet_Create(MSG_CREATE_RESPUESTA, 2);
    Packet_Append(respuesta, resultadoCreate);
    Socket_SendPacket(s, respuesta);
    Packet_Destroy(respuesta);

    free(nombreTabla);

}


void HandleDescribeOpcode(Socket* s, Packet* p)
{
    bool tablePresent;
    Packet_Read(p, &tablePresent);

    char* nombreTabla = NULL;
    if (tablePresent)
        Packet_Read(p, &nombreTabla);;
    t_describe* elemento;
    Packet* resp;

    if (nombreTabla == NULL)
    {
        t_list* resDescribe = describe(nombreTabla);
        int tam = list_size(resDescribe) * (sizeof(t_describe));
        resp = Packet_Create(MSG_DESCRIBE_GLOBAL, tam);
        Packet_Append(resp, list_size(resDescribe));
        int i = 0;
        while (i < list_size(resDescribe))
        {
            elemento = list_get(resDescribe, i);
            Packet_Append(resp, elemento->table);
            Packet_Append(resp, elemento->consistency);
            Packet_Append(resp, elemento->partitions);
            Packet_Append(resp, elemento->compaction_time);
            ++i;
        }
    }
    else
    {
        elemento = describe(nombreTabla);
        int tam = sizeof(t_describe);
        resp = Packet_Create(MSG_DESCRIBE, tam);
        Packet_Append(resp, nombreTabla);
        Packet_Append(resp, elemento->consistency);
        Packet_Append(resp, elemento->partitions);
        Packet_Append(resp, elemento->compaction_time);
    }

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);

    // dummy asi puedo probar!
    /*
    Packet* resp = Packet_Create(MSG_DESCRIBE, 50);
    Packet_Append(resp, "PROBANDO");
    Packet_Append(resp, (uint8_t) CRITERIA_SC);
    Packet_Append(resp, (uint16_t) 1);
    Packet_Append(resp, (uint32_t) 30000);

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);
     */
}

void HandleDropOpcode(Socket* s, Packet* p)
{
    char* nombreTabla;

    Packet_Read(p, &nombreTabla);

    int resultadoDrop = drop(nombreTabla);

    if (resultadoDrop == EXIT_FAILURE)
    {
        LISSANDRA_LOG_ERROR("No se pudo hacer drop de la tabla: %s", nombreTabla);
    }

    //Hay que crear un MSG_DROP?????
    Packet* respuesta = Packet_Create(MSG_DROP_RESPUESTA, 2);
    Packet_Append(respuesta, resultadoDrop);
    Socket_SendPacket(s, respuesta);
    Packet_Destroy(respuesta);

    free(nombreTabla);

}
