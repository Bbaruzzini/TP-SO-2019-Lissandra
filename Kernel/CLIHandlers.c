#include "CLIHandlers.h"
#include "Criteria.h"
#include <File.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void HandleSelect(char const* args)
{
    (void) args;
}

void HandleInsert(char const* args)
{
    (void) args;
}

void HandleCreate(char const* args)
{
    (void) args;
}

void HandleDescribe(char const* args)
{
    (void) args;
}

void HandleDrop(char const* args)
{
    (void) args;
}

void HandleJournal(char const* args)
{
    (void) args;
}

void HandleAdd(char const* args)
{
    if (!*args)
    {
        LISSANDRA_LOG_ERROR("ADD: Parámetros insuficientes.");
        return;
    }

    //           cmd args
    //               0      1   2  3
    // sintaxis: ADD MEMORY <n> TO <criteria>

    char** tokens = string_split(args, " ");

    char* magic = tokens[0];
    if (!magic)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        Free(tokens);
        return;
    }

    if (strcmp(magic, "MEMORY"))
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        Free(magic);
        Free(tokens);
        return;
    }

    char* idx = tokens[1];
    if (!idx)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        Free(magic);
        Free(tokens);
        return;
    }

    int memIdx = atoi(idx);
    if (!memIdx)
    {
        LISSANDRA_LOG_ERROR("ADD: Memoria %s no válida", idx);
        Free(idx);
        Free(magic);
        Free(tokens);
        return;
    }

    char* magic2 = tokens[2];
    if (!magic2)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        Free(idx);
        Free(magic);
        Free(tokens);
        return;
    }

    if (strcmp(magic2, "TO"))
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        Free(magic2);
        Free(idx);
        Free(magic);
        Free(tokens);
        return;
    }

    char* criteria = tokens[3];
    if (!criteria)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        Free(magic2);
        Free(idx);
        Free(magic);
        Free(tokens);
        return;
    }

    CriteriaType type;
    if (!strcmp(criteria, "SC"))
        type = CRITERIA_SC;
    else if (!strcmp(criteria, "SHC"))
        type = CRITERIA_SHC;
    else if (!strcmp(criteria, "EC"))
        type = CRITERIA_EC;
    else
    {
        LISSANDRA_LOG_ERROR("ADD: Criterio %s no válido. Criterios validos: SC - SHC - EC.", criteria);
        Free(criteria);
        Free(magic2);
        Free(idx);
        Free(magic);
        Free(tokens);
        return;
    }

    //Socket* s = MemoryMgr_GetConnection(memIdx);
    Criteria_AddMemory(type, memIdx, NULL /*s*/);

    Free(criteria);
    Free(magic2);
    Free(idx);
    Free(magic);
    Free(tokens);
}

void HandleRun(char const* args)
{
    if (!*args)
    {
        LISSANDRA_LOG_ERROR("Sintaxis incorrecta.");
        return;
    }

    //           cmd args
    //               0
    // sintaxis: RUN <path>
    File* script = file_open(args, F_OPEN_READ);
    if (!file_is_open(script))
    {
        LISSANDRA_LOG_ERROR("Archivo no válido: %s", args);
        file_close(script);
        return;
    }

    //Planificador_AgregarScript(script);
}

void HandleMetrics(char const* args)
{
    (void) args;
}
