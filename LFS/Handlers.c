
#include "Handlers.h"
#include "API.h"
#include "Config.h"
#include <Consistency.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <stdlib.h>
#include <Timer.h>

OpcodeHandlerFnType* const OpcodeTable[NUM_HANDLED_OPCODES] =
{
    // handshake memoria manejado en memoria_conectar
    NULL,                   // MSG_HANDSHAKE

    // 5 comandos basicos
    HandleSelectOpcode,     // LQL_SELECT
    HandleInsertOpcode,     // LQL_INSERT
    HandleCreateOpcode,     // LQL_CREATE
    HandleDescribeOpcode,   // LQL_DESCRIBE
    HandleDropOpcode,       // LQL_DROP

    // mensaje a memoria, ignoramos
    NULL                    // LQL_JOURNAL
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

    Packet* respuesta = Packet_Create(MSG_SELECT, confLFS.TAMANIO_VALUE);
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
    uint64_t timestamp;
    Packet_Read(p, &nombreTabla);
    Packet_Read(p, &key);
    Packet_Read(p, &value);
    Packet_Read(p, &timestamp);

    //Hay que agregar algo al define de Packet_Read?????
    //Packet_Read(p, &timestamp);

    //resultadoInsert es EXIT_SUCCESS o EXIT_FAILURE
    uint8_t resultadoInsert = api_insert(nombreTabla, key, value, timestamp);
    if (resultadoInsert == EXIT_FAILURE)
        LISSANDRA_LOG_ERROR("No se pudo realizar el insert de: %s", nombreTabla);

    Packet* respuesta = Packet_Create(MSG_INSERT_RESPUESTA, 1);
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

    uint8_t resultadoCreate = api_create(nombreTabla, tipoConsistencia, numeroParticiones, compactionTime);
    if (resultadoCreate == EXIT_FAILURE)
        LISSANDRA_LOG_ERROR("No se pudo crear la tabla: %s", nombreTabla);

    //Hay que crear un MSG_CREATE?????
    Packet* respuesta = Packet_Create(MSG_CREATE_RESPUESTA, 1);
    Packet_Append(respuesta, resultadoCreate);
    Socket_SendPacket(s, respuesta);
    Packet_Destroy(respuesta);

    free(nombreTabla);
}

void _addToPacket(void* elem, void* packet)
{
    t_describe* const elemento = elem;
    Packet_Append(packet, elemento->table);
    Packet_Append(packet, elemento->consistency);
    Packet_Append(packet, elemento->partitions);
    Packet_Append(packet, elemento->compaction_time);
}

void HandleDescribeOpcode(Socket* s, Packet* p)
{
    bool tablePresent;
    Packet_Read(p, &tablePresent);

    char* nombreTabla = NULL;
    if (tablePresent)
        Packet_Read(p, &nombreTabla);
    Packet* resp;

    if (!nombreTabla)
    {
        t_list* resDescribe = api_describe(nombreTabla);
        uint32_t elementos = list_size(resDescribe);

        size_t const tam = 4 + elementos * sizeof(t_describe);
        resp = Packet_Create(MSG_DESCRIBE_GLOBAL, tam);

        Packet_Append(resp, elementos);
        list_iterate_with_data(resDescribe, _addToPacket, resp);
        list_destroy_and_destroy_elements(resDescribe, Free);
    }
    else
    {
        t_describe* elemento = api_describe(nombreTabla);

        size_t const tam = sizeof(t_describe);
        resp = Packet_Create(MSG_DESCRIBE, tam);

        _addToPacket(elemento, resp);
        Free(elemento);
    }

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);
}

void HandleDropOpcode(Socket* s, Packet* p)
{
    char* nombreTabla;

    Packet_Read(p, &nombreTabla);

    uint8_t resultadoDrop = api_drop(nombreTabla);
    if (resultadoDrop == EXIT_FAILURE)
        LISSANDRA_LOG_ERROR("No se pudo hacer drop de la tabla: %s", nombreTabla);

    //Hay que crear un MSG_DROP?????
    Packet* respuesta = Packet_Create(MSG_DROP_RESPUESTA, 1);
    Packet_Append(respuesta, resultadoDrop);
    Socket_SendPacket(s, respuesta);
    Packet_Destroy(respuesta);

    free(nombreTabla);
}
