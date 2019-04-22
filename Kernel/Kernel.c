
#include "Appender.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "Console.h"
#include "CLIHandlers.h"
#include "EventDispatcher.h"
#include "libcommons/string.h"
#include "Logger.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>

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

char const* CLIPrompt = "KERNEL> ";

LockedQueue* CLICommandQueue = NULL;

atomic_bool ProcessRunning = true;

static void _gracefulExit(int signo)
{
    (void) signo;
    ProcessRunning = false;
}

int main(void)
{
    // logger initialization
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    Appender* consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, "198EDC");
    Logger_AddAppender(consoleLog);

    AppenderFlags const fileFlags = consoleFlags | APPENDER_FLAGS_USE_TIMESTAMP | APPENDER_FLAGS_MAKE_FILE_BACKUP;
    Appender* fileLog = AppenderFile_Create(LOG_LEVEL_ERROR, fileFlags, "kernel.log", "w", 0);
    Logger_AddAppender(fileLog);

    // dispatcher initialization
    {
        if (!EventDispatcher_Init())
            exit(1);
    }

    struct sigaction sa = { 0 };
    sa.sa_handler = _gracefulExit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        LISSANDRA_LOG_FATAL("No pude registrar el handler de señales! Saliendo...");
        return EXIT_FAILURE;
    }

    CLICommandQueue = LockedQueue_Create();

    // subimos el nivel a errores para no entorpecer la consola
    Appender_SetLogLevel(consoleLog, LOG_LEVEL_ERROR);

    // el kokoro
    {
        pthread_t consoleTid;
        pthread_create(&consoleTid, NULL, CliThread, NULL);

        EventDispatcher_Loop();

        pthread_join(consoleTid, NULL);
    }

    // cleanup
    LockedQueue_Destroy(CLICommandQueue, Free);
    EventDispatcher_Terminate();
    Logger_Terminate();
}
