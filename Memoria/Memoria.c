#include "Appender.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "CLIHandlers.h"
#include "Config.h"
#include "Console.h"
#include "EventDispatcher.h"
#include "FileWatcher.h"
#include "Logger.h"
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

CLICommand CLICommands[] =
{
    { "SELECT",   HandleSelect   },
    { "INSERT",   HandleInsert   },
    { "CREATE",   HandleCreate   },
    { "DESCRIBE", HandleDescribe },
    { "DROP",     HandleDrop     },
    { "JOURNAL",  HandleJournal  },
    { NULL,       NULL           }
};

int main(void)
{

}
