
#include "CLIHandlers.h"
#include "Appender.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "EventDispatcher.h"
#include "libcommons/string.h"
#include "Logger.h"
#include "LockedQueue.h"
#include "Timer.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>

#define SLEEP_CONST 50

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

        void MainLoop(void);
        MainLoop();

        pthread_join(consoleTid, NULL);
    }

    // cleanup
    LockedQueue_Destroy(CLICommandQueue, Free);
    EventDispatcher_Terminate();
    Logger_Terminate();
}

void AtenderComando(char const* command)
{
    size_t spc = strcspn(command, " ");
    char* cmd = Malloc(spc + 1);
    strncpy(cmd, command, spc + 1);
    cmd[spc] = '\0';

    command = command + spc;

    //skip any extra spaces
    while (*command == ' ')
        ++command;

    for (uint32_t i = 0; CLICommands[i].CmdName != NULL; ++i)
    {
        if (!strcmp(CLICommands[i].CmdName, cmd))
        {
            CLICommands[i].Handler(command);
            break;
        }
    }

    Free(cmd);
}

void MainLoop(void)
{
    uint32_t realCurrTime = 0;

    while (ProcessRunning)
    {
        realCurrTime = GetMSTime();

        // procesar comandos
        char* command;
        while ((command = LockedQueue_Next(CLICommandQueue)))
        {
            AtenderComando(command);
            Free(command);
        }

        // procesar fds (sockets, inotify, etc...)
        EventDispatcher_Dispatch();

        // si atendimos rapido ponemos a dormir un rato mas para no quemar la cpu (?
        uint32_t executionTimeDiff = GetMSTimeDiff(realCurrTime, GetMSTime());
        if (executionTimeDiff < SLEEP_CONST)
            MSSleep(SLEEP_CONST - executionTimeDiff);
    }
}
