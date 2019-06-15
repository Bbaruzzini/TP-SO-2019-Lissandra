//
// Created by Denise on 09/06/2019.
//

#include "CLIHandlers.h"

static inline bool ValidateKey(char const* keyString, uint16_t* result)
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

//Funcion para cambiar el timestamp de char* a time_t
static inline void ChangeTimestamp(char const* timestampString, time_t* result)
{
    uint32_t ts = strtoul(timestampString, NULL, 10);

    *result = (time_t) ts;
}

void HandleSelect(Vector const* args)
{
    //           cmd args
    //           0      1       2
    // sintaxis: SELECT <table> <key>

    if (Vector_size(args) != 3)
    {
        LISSANDRA_LOG_ERROR("SELECT: Uso - SELECT <tabla> <key>");
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];

    uint16_t k;
    if (!ValidateKey(key, &k))
        return;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Select.Key = k;

    //TODO: completar con lo que falta desde aca
    //select_api(dbr.TableName, dbr.Data.Select.Key);

}

void HandleInsert(Vector const* args)
{
    //           cmd args
    //           0      1       2     3        4 (opcional)
    // sintaxis: INSERT <table> <key> <value> <timestamp>

    if (Vector_size(args) > 5)
    {
        LISSANDRA_LOG_ERROR("INSERT: Uso - INSERT <table> <key> <value> <timestamp>");
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

    time_t ts;

    if (timestamp != NULL)
    {
        ChangeTimestamp(timestamp, &ts);
    }
    else
    {
        ts = 0;
    }

    DBRequest dbr;

    dbr.TableName = table;
    dbr.Data.Insert.Key = k;
    dbr.Data.Insert.Value = value;
    dbr.Data.Insert.Timestamp = ts;

    int resultadoInsert = insert(dbr.TableName, dbr.Data.Insert.Key, dbr.Data.Insert.Value, dbr.Data.Insert.Timestamp);

    if (resultadoInsert == EXIT_SUCCESS)
    {
        LISSANDRA_LOG_INFO("Se completo INSERT a la tabla: %s", table);
    }
    else
    {
        LISSANDRA_LOG_ERROR("No se pudo realizar el INSERT: la tabla ingresada no existe en el File System");
    }

    //Deberia liberar a dbr???

}

void HandleCreate(Vector const* args)
{
    //           cmd args
    //           0      1       2          3            4
    // sintaxis: CREATE <table> <consistency> <partitions> <compaction_time>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("CREATE: Uso - CREATE <tabla> <consistencia> <particiones> <tiempo entre compactaciones>");
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const consistency = tokens[2];
    char* const partitions = tokens[3];
    char* const compaction_time = tokens[4];

    DBRequest dbr;

    dbr.TableName = table;
    dbr.Data.Create.Consistency = consistency;
    dbr.Data.Create.Partitions = strtoul(partitions, NULL, 10);
    dbr.Data.Create.CompactTime = strtoul(compaction_time, NULL, 10);

    int resultadoCreate = create(dbr.TableName, dbr.Data.Create.Consistency, dbr.Data.Create.Partitions,
                                 dbr.Data.Create.CompactTime);

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
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
        table = tokens[1];

    DBRequest dbr;

    dbr.TableName = table;

    if (table == NULL)
    {
        t_describe* elemento;
        int i = 0;
        t_list* resultadoDescribeNull = Malloc(sizeof(t_describe));
        resultadoDescribeNull = describe(dbr.TableName);
        while (i < list_size(resultadoDescribeNull))
        {
            elemento = list_get(resultadoDescribeNull, i);
            printf("Tabla: %s\n", elemento->table);
            printf("Consistencia: %s\n", elemento->consistency);
            printf("Particiones: %d\n", elemento->partitions);
            printf("Tiempo: %d\n", elemento->compaction_time);
            i++;
        }
        free(resultadoDescribeNull);
    }
    else
    {
        t_describe* resultadoDescribe = describe(dbr.TableName);
        printf("Tabla: %s\n", resultadoDescribe->table);
        printf("Consistencia: %s\n", resultadoDescribe->consistency);
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
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];

    DBRequest dbr;

    dbr.TableName = table;

    int resultado = drop(dbr.TableName);

    if (resultado == EXIT_SUCCESS)
    {
        printf("Se borro con exito la tabla: %s\n", table);
    }
    else
    {
        printf("Se produjo un error intentando borrar la tabla: %s\n", table);
    }

}
