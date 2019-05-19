
#include "Criteria.h"
#include "Metadata.h"
#include "Runner.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <Config.h>
#include <Console.h>
#include <EventDispatcher.h>
#include <FileWatcher.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

char const* CLIPrompt = "KRNL_LISSANDRA> ";

LockedQueue* CLICommandQueue = NULL;

atomic_bool ProcessRunning = true;

static Appender* consoleLog;
static Appender* fileLog;

static void SignalTrap(int signal)
{
    (void) signal;
    ProcessRunning = false;
}

static void IniciarLogger(void)
{
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, "198EDC");
    Logger_AddAppender(consoleLog);

    AppenderFlags const fileFlags = consoleFlags | APPENDER_FLAGS_USE_TIMESTAMP | APPENDER_FLAGS_MAKE_FILE_BACKUP;
    fileLog = AppenderFile_Create(LOG_LEVEL_ERROR, fileFlags, "kernel.log", "w", 0);
    Logger_AddAppender(fileLog);
}

static void IniciarDispatch(void)
{
    if (!EventDispatcher_Init())
        exit(EXIT_FAILURE);
}

static void Trap(int signal)
{
    struct sigaction sa = { 0 };
    sa.sa_handler = SignalTrap;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(signal, &sa, NULL) < 0)
    {
        LISSANDRA_LOG_FATAL("No pude registrar el handler de señales! Saliendo...");
        exit(EXIT_FAILURE);
    }
}

static void LoadConfig(char const* fileName)
{
    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);
    pthread_rwlock_wrlock(&sConfigLock);
    if (sConfig)
        config_destroy(sConfig);

    sConfig = config_create(fileName);
    pthread_rwlock_unlock(&sConfigLock);
}

static void SetupConfigInitial(char const* fileName)
{
    LoadConfig(fileName);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, fileName, LoadConfig);
    EventDispatcher_AddFDI(fw);
}

static void InitConsole(void)
{
    CLICommandQueue = LockedQueue_Create();

    // subimos el nivel a errores para no entorpecer la consola
    Appender_SetLogLevel(consoleLog, LOG_LEVEL_ERROR);
}

static void InitMemorySubsystem(void)
{
    Criterias_Init();
    Metadata_Init();
}

static void MainLoop(void)
{
    // el kokoro
    pthread_t consoleTid;
    pthread_create(&consoleTid, NULL, CliThread, NULL);
    Runner_Init();

    EventDispatcher_Loop();

    Runner_Terminate();
    pthread_join(consoleTid, NULL);
}

static void Cleanup(void)
{
    Metadata_Destroy();
    Criterias_Destroy();
    LockedQueue_Destroy(CLICommandQueue, Free);
    EventDispatcher_Terminate();
    Logger_Terminate();
}

int main(void)
{
    static char const* const configFileName = "kernel.conf";

    IniciarLogger();
    IniciarDispatch();
    Trap(SIGINT);
    SetupConfigInitial(configFileName);

    InitConsole();

    InitMemorySubsystem();

    MainLoop();

    Cleanup();
}
