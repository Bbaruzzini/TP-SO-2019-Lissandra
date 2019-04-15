
#include "CLIHandlers.h"
#include <stddef.h>

CLICommand CLICommands[] =
{
    { "SELECT",   HandleSelect   },
    { "INSERT",   HandleInsert   },
    { "CREATE",   HandleCreate   },
    { "DESCRIBE", HandleDescribe },
    { "DROP",     HandleDrop     },
    { "JOURNAL",  HandleJournal  },
    { "ADD",      HandleAdd      },
    { "RUN",      HandleRun      },
    { "METRICS",  HandleMetrics  },
    { NULL,       NULL           }
};

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
    (void) args;
}

void HandleRun(char const* args)
{
    (void) args;
}

void HandleMetrics(char const* args)
{
    (void) args;
}
