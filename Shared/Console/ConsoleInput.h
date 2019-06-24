
#ifndef ConsoleInput_h__
#define ConsoleInput_h__

#include "Logger.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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

static inline bool ValidateTableName(char const* const tableName)
{
    bool res = true;
    for (char const* itr = tableName; *itr; ++itr)
    {
        if (!isalnum(*itr))
        {
            LISSANDRA_LOG_ERROR("Tabla %s invalida", tableName);
            res = false;
            break;
        }
    }

    return res;
}

#endif //ConsoleInput_h__
