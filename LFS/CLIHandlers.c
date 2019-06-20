//
// Created by Denise on 09/06/2019.
//

#include "CLIHandlers.h"
#include "API.h"
#include <Consistency.h>
#include <stdlib.h>
#include <Timer.h>

bool ValidateKey(char const* keyString, uint16_t* result)
{
    errno = 0;
    uint32_t k = strtoul(keyString, NULL, 10);
    if (errno || k > UINT16_MAX)
    {
        LISSANDRA_LOG_ERROR("Key %s invalida", keyString);
        return false;
    }

    *result = (uint16_t) k;
    return true;
}

void HandleSelect(Vector const* args)
{
    //           cmd args
    //           0      1       2
    // sintaxis: SELECT <table> <key>

    if (Vector_size(args) != 3)
    {
        LISSANDRA_LOG_ERROR("SELECT: Uso - SELECT <tabla> <key>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];

    uint16_t k;
    if (!ValidateKey(key, &k))
        return;

    //TODO: completar con lo que falta desde aca
    select_api(table, k);
}

void HandleInsert(Vector const* args)
{
    //           cmd args
    //           0      1       2     3        4 (opcional)
    // sintaxis: INSERT <table> <key> <value> <timestamp>

    if (Vector_size(args) > 5)
    {
        LISSANDRA_LOG_ERROR("INSERT: Uso - INSERT <table> <key> <value> <timestamp>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];
    char* const value = tokens[3];

    char* timestamp = NULL;
    if (Vector_size(args) == 5)
        timestamp = tokens[4];

    uint16_t k;
    if (!ValidateKey(key, &k))
        return;

    uint64_t ts = GetMSEpoch();
    if (timestamp != NULL)
        ts = strtoull(timestamp, NULL, 10);

    uint8_t resultadoInsert = insert(table, k, value, ts);
    if (resultadoInsert == EXIT_SUCCESS)
    {
        LISSANDRA_LOG_INFO("Se completo INSERT a la tabla: %s", table);
    }
    else
    {
        LISSANDRA_LOG_ERROR("No se pudo realizar el INSERT: la tabla ingresada no existe en el File System");
    }
}

void HandleCreate(Vector const* args)
{
    //           cmd args
    //           0      1       2          3            4
    // sintaxis: CREATE <table> <consistency> <partitions> <compaction_time>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("CREATE: Uso - CREATE <tabla> <consistencia> <particiones> <tiempo entre compactaciones>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const consistency = tokens[2];
    char* const partitions = tokens[3];
    char* const compaction_time = tokens[4];

    CriteriaType ct; // ya se loguea el error
    if (!CriteriaFromString(consistency, &ct))
        return;

    uint32_t const parts = strtoul(partitions, NULL, 10);
    uint32_t const compTime = strtoul(compaction_time, NULL, 10);

    uint8_t resultadoCreate = create(table, ct, parts, compTime);
    if (resultadoCreate == EXIT_SUCCESS)
    {
        printf("La tabla %s se creo con exito\n", table);
    }
    else
    {
        printf("No se pudo crear la tabla %s\n", table);
    }

}

void HandleDescribe(Vector const* args)
{
    //           cmd args
    //           0        1
    // sintaxis: DESCRIBE <name>

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DESCRIBE: Uso - DESCRIBE <tabla>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
        table = tokens[1];

    if (table == NULL)
    {
        t_describe* elemento;
        size_t i = 0;
        t_list* resultadoDescribeNull = describe(NULL);
        while (i < list_size(resultadoDescribeNull))
        {
            elemento = list_get(resultadoDescribeNull, i);
            printf("Tabla: %s\n", elemento->table);
            printf("Consistencia: %s\n", CriteriaString[elemento->consistency].String);
            printf("Particiones: %d\n", elemento->partitions);
            printf("Tiempo: %d\n", elemento->compaction_time);
            ++i;
        }
        free(resultadoDescribeNull);
    }
    else
    {
        t_describe* resultadoDescribe = describe(table);
        printf("Tabla: %s\n", resultadoDescribe->table);
        printf("Consistencia: %s\n", CriteriaString[resultadoDescribe->consistency].String);
        printf("Particiones: %d\n", resultadoDescribe->partitions);
        printf("Tiempo: %d\n", resultadoDescribe->compaction_time);
        free(resultadoDescribe);
    }
}

void HandleDrop(Vector const* args)
{
    //           cmd args
    //           0      1
    // sintaxis: DROP <table>

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DROP: Uso - DROP <table>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];

    uint8_t resultado = drop(table);

    if (resultado == EXIT_SUCCESS)
    {
        printf("Se borro con exito la tabla: %s\n", table);
    }
    else
    {
        printf("Se produjo un error intentando borrar la tabla: %s\n", table);
    }

}
