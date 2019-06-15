//
// Created by Denise on 09/06/2019.
//

#ifndef LISSANDRA_CLIHANDLERS_H
#define LISSANDRA_CLIHANDLERS_H

#include <Console.h>
#include <string.h>
#include <stdlib.h>
#include <libcommons/string.h>
#include <Logger.h>
#include "API.h"

typedef struct
{
    char /*const*/* TableName;
    union
    {
        struct
        {
            uint16_t Key;
        } Select;

        struct
        {
            uint16_t Key;
            char /*const*/* Value;
            time_t Timestamp;
        } Insert;

        struct
        {
            /*CriteriaType*/char* Consistency;
            uint16_t Partitions;
            uint32_t CompactTime;
        } Create;
    } Data;
} DBRequest;

static inline bool ValidateKey(char const* keyString, uint16_t* result);
static inline void ChangeTimestamp(char const* timestampString, time_t* result);

CLICommandHandlerFn HandleSelect;
CLICommandHandlerFn HandleInsert;
CLICommandHandlerFn HandleCreate;
CLICommandHandlerFn HandleDescribe;
CLICommandHandlerFn HandleDrop;

#endif //LISSANDRA_CLIHANDLERS_H
