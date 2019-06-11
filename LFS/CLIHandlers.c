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
    (void) args;
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
    dbr.Data.Insert.Timestamp = ts;

    //TODO: resto de la funcion

}

void HandleCreate(Vector const* args)
{
    (void) args;
}

void HandleDescribe(Vector const* args)
{/*
    //           cmd args
    //           0        1
    // sintaxis: DESCRIBE [name]

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DESCRIBE: Uso - DESCRIBE [tabla]");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
        table = tokens[1];

    DBRequest dbr;
    dbr.TableName = table;

    //ACA VA LA FUNCION DE DESCRIBE
  Metadata_Clear();  */ //ESTO LO PONEMOS POR LAS DUDAS PARA IR VIENDO SI ES NECESARIO O NO.
}

void HandleDrop(Vector const* args)
{
    (void) args;
}
