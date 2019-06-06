
#include "CLIHandlers.h"
#include "FileSystemSocket.h"
#include "MainMemory.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <Config.h>
#include <EventDispatcher.h>
#include <FileWatcher.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

CLICommand const CLICommands[] =
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

Socket* FileSystemSocket = NULL;

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

static void MainLoop(void)
{
    // el kokoro
    pthread_t consoleTid;
    pthread_create(&consoleTid, NULL, CliThread, NULL);

    EventDispatcher_Loop();

    pthread_join(consoleTid, NULL);
}

static void DoHandshake(uint32_t* maxValueLength, char** mountPoint)
{
    pthread_rwlock_rdlock(&sConfigLock);
    char* fs_ip = config_get_string_value(sConfig, "IP_FS");
    char* fs_port = config_get_string_value(sConfig, "PUERTO_FS");

    SocketOpts const so =
    {
        .HostName = fs_ip,
        .ServiceOrPort = fs_port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    FileSystemSocket = Socket_Create(&so);
    pthread_rwlock_unlock(&sConfigLock);

    if (!FileSystemSocket)
    {
        LISSANDRA_LOG_FATAL("No pude conectarme al FileSystem!!");
        exit(1);
    }

    static uint8_t const id = MEMORIA;
    Packet* p = Packet_Create(MSG_HANDSHAKE, 1);
    Packet_Append(p, id);
    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(FileSystemSocket);
    if (Packet_GetOpcode(p) != MSG_HANDSHAKE_RESPUESTA)
    {
        LISSANDRA_LOG_FATAL("FileSystem envió respuesta inválida.");
        exit(1);
    }

    Packet_Read(p, maxValueLength);
    Packet_Read(p, mountPoint);
    Packet_Destroy(p);
}

static void StartMemory(void)
{
    uint32_t maxValueLength;
    char* mountPoint;
    DoHandshake(&maxValueLength, &mountPoint);
    Memory_Initialize(maxValueLength, mountPoint);
    Free(mountPoint);
}

static void StartGossip(void)
{
    //todo
    //?
    //profit
}

static void Cleanup(void)
{
    LockedQueue_Destroy(CLICommandQueue, Free);
    EventDispatcher_Terminate();
    Logger_Terminate();
}

int main(void)
{
    static char const configFileName[] = "memoria.conf";
    IniciarLogger();
    IniciarDispatch();
    SigintSetup();
    SetupConfigInitial(configFileName);

    InitConsole();

    StartMemory();
    StartGossip();

    MainLoop();

    Cleanup();
}
