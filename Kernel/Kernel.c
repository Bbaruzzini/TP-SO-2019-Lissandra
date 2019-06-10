
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
#include <Opcodes.h>
#include <Packet.h>
#include <pthread.h>
#include <Socket.h>
#include <stdbool.h>
#include <stdlib.h>

char const* CLIPrompt = "KRNL_LISSANDRA> ";

LockedQueue* CLICommandQueue = NULL;

atomic_bool ProcessRunning = true;

static Appender* consoleLog;
static Appender* fileLog;

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
    //Appender_SetLogLevel(consoleLog, LOG_LEVEL_ERROR);
}

static void InitMemorySubsystem(void)
{
    Criterias_Init();
    Metadata_Init();

    char* const seed_ip = config_get_string_value(sConfig, "IP_MEMORIA");
    char* const seed_port = config_get_string_value(sConfig, "PUERTO_MEMORIA");

    SocketOpts const so =
    {
        .HostName = seed_ip,
        .ServiceOrPort = seed_port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    Socket* s = Socket_Create(&so);
    if (!s)
    {
        LISSANDRA_LOG_FATAL("No pude conectarme a la memoria semilla!!");
        exit(1);
    }

    static uint8_t const id = KERNEL;
    Packet* p = Packet_Create(MSG_HANDSHAKE, 1);
    Packet_Append(p, id);
    Socket_SendPacket(s, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(s);
    if (Packet_GetOpcode(p) != MSG_MEMORY_ID)
    {
        LISSANDRA_LOG_FATAL("Memoria envió respuesta inválida.");
        exit(1);
    }

    uint32_t memId;
    Packet_Read(p, &memId);
    Packet_Destroy(p);

    Criteria_ConnectMemory(memId, s);
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
    static char const configFileName[] = "kernel.conf";

    IniciarLogger();
    IniciarDispatch();
    SigintSetup();
    SetupConfigInitial(configFileName);

    InitConsole();

    InitMemorySubsystem();

    MainLoop();

    Cleanup();
}
