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

char const* CLIPrompt = "MEM_LISSANDRA> ";

LockedQueue* CLICommandQueue = NULL;

atomic_bool ProcessRunning = true;

static Appender* consoleLog;
static Appender* fileLog;

static void IniciarLogger(void)
{
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    //Se establece el mismo color que el kernel "EA899A""
    consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, "EA899A");
    Logger_AddAppender(consoleLog);

    AppenderFlags const fileFlags = consoleFlags | APPENDER_FLAGS_USE_TIMESTAMP | APPENDER_FLAGS_MAKE_FILE_BACKUP;
    fileLog = AppenderFile_Create(LOG_LEVEL_ERROR, fileFlags, "memoria.log", "w", 0);
    Logger_AddAppender(fileLog);
}

static void LoadConfig(char const* fileName)
{
    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);
    if (sConfig)
        config_destroy(sConfig);

    sConfig = config_create(fileName);
}

int main(void)
{
    static char const* const configFileName = "memoria.conf";

    IniciarLogger();
    LoadConfig(configFileName);
}
