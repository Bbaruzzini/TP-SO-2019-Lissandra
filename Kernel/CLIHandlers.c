
#include "CLIHandlers.h"
#include "Criteria.h"
#include <File.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void HandleSelect(Vector const* args)
{
    (void) args;
}

void HandleInsert(Vector const* args)
{
    (void) args;
}

void HandleCreate(Vector const* args)
{
    (void) args;
}

void HandleDescribe(Vector const* args)
{
    (void) args;
}

void HandleDrop(Vector const* args)
{
    (void) args;
}

void HandleJournal(Vector const* args)
{
    (void) args;
}

void HandleAdd(Vector const* args)
{
    //           cmd args
    //               0      1   2  3
    // sintaxis: ADD MEMORY <n> TO <criteria>

    if (Vector_size(args) != 4)
    {
        LISSANDRA_LOG_ERROR("ADD: Cantidad de parámetros incorrecta.");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const magic = tokens[0];    // MEMORY
    char* const idx = tokens[1];      // n
    char* const magic2 = tokens[2];   // TO
    char* const criteria = tokens[3]; // criteria

    if (strcmp(magic, "MEMORY") != 0)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return;
    }

    uint32_t memIdx = strtoul(idx, NULL, 10);
    if (!memIdx)
    {
        LISSANDRA_LOG_ERROR("ADD: Memoria %s no válida", idx);
        return;
    }

    if (strcmp(magic2, "TO") != 0)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return;
    }

    struct CriteriaString
    {
        char const* String;
        CriteriaType Criteria;
    };

    static struct CriteriaString const cs[NUM_CRITERIA] =
    {
        { "SC",  CRITERIA_SC },
        { "SHC", CRITERIA_SHC },
        { "EC",  CRITERIA_EC }
    };

    CriteriaType type;
    uint8_t i = 0;
    for (; i < NUM_CRITERIA; ++i)
    {
        if (!strcmp(criteria, cs[i].String))
        {
            type = cs[i].Criteria;
            break;
        }
    }

    if (i == NUM_CRITERIA)
    {
        LISSANDRA_LOG_ERROR("ADD: Criterio %s no válido. Criterios validos: SC - SHC - EC.", criteria);
        return;
    }

    //Socket* s = MemoryMgr_GetConnection(memIdx);
    Criteria_AddMemory(type, NULL /*s*/);
}

void HandleRun(Vector const* args)
{
    //           cmd args
    //               0
    // sintaxis: RUN <path>
    if (Vector_size(args) != 1)
    {
        LISSANDRA_LOG_ERROR("RUN: Cantidad de parámetros incorrecta.");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const fileName = tokens[0];
    File* script = file_open(fileName, F_OPEN_READ);
    if (!file_is_open(script))
    {
        LISSANDRA_LOG_ERROR("RUN: Archivo no válido: %s", fileName);
        file_close(script);
        return;
    }

    //Planificador_AgregarScript(script);
}

void HandleMetrics(Vector const* args)
{
    //           cmd
    // sintaxis: METRICS
    if (Vector_size(args) != 0)
    {
        LISSANDRA_LOG_ERROR("RUN: Cantidad de parámetros incorrecta.");
        return;
    }

    Criterias_Report();
}
