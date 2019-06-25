
#include "API.h"
#include "CLIHandlers.h"
#include "FileSystemSocket.h"
#include "Gossip.h"
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
#include <Timer.h>

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

atomic_bool ProcessRunning = true;

static Appender* consoleLog = NULL;
static Appender* fileLog = NULL;

static Socket* ListeningSocket = NULL;

static PeriodicTimer* JournalTimer = NULL;
static PeriodicTimer* GossipTimer = NULL;

Socket* FileSystemSocket = NULL;

static void IniciarLogger(void)
{
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, LGREEN, LRED, LRED, YELLOW, LGREEN, WHITE);
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
    if (sConfig)
        config_destroy(sConfig);

    sConfig = config_create(fileName);

    // recargar los timers
    PeriodicTimer_ReSetTimer(JournalTimer, config_get_long_value(sConfig, "RETARDO_JOURNAL"));
    PeriodicTimer_ReSetTimer(GossipTimer, config_get_long_value(sConfig, "RETARDO_GOSSIPING"));
}

static void SetupConfigInitial(char const* fileName)
{
    JournalTimer = PeriodicTimer_Create(0, API_Journal);
    GossipTimer = PeriodicTimer_Create(0, Gossip_Do);

    LoadConfig(fileName);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, fileName, LoadConfig);
    EventDispatcher_AddFDI(fw);

    // agregar timers asi tengo notificaciones
    EventDispatcher_AddFDI(JournalTimer);
    EventDispatcher_AddFDI(GossipTimer);
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
    char* const fs_ip = config_get_string_value(sConfig, "IP_FS");
    char* const fs_port = config_get_string_value(sConfig, "PUERTO_FS");

    SocketOpts const so =
    {
        .HostName = fs_ip,
        .ServiceOrPort = fs_port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    FileSystemSocket = Socket_Create(&so);

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

    char const* const listen_port = config_get_string_value(sConfig, "PUERTO");

    SocketOpts const so =
    {
        .HostName = NULL,
        .ServiceOrPort = listen_port,
        .SocketMode = SOCKET_SERVER,
        .SocketOnAcceptClient = NULL
    };
    ListeningSocket = Socket_Create(&so);

    if (!ListeningSocket)
    {
        LISSANDRA_LOG_FATAL("No pudo iniciarse el socket de escucha!!");
        exit(1);
    }

    EventDispatcher_AddFDI(ListeningSocket);
}

static void StartGossip(void)
{
    Vector ips = config_get_array_value(sConfig, "IP_SEEDS");
    Vector ports = config_get_array_value(sConfig, "PUERTO_SEEDS");

    uint32_t id = config_get_long_value(sConfig, "MEMORY_NUMBER");
    char const* const listen_port = config_get_string_value(sConfig, "PUERTO");

    Gossip_Init(&ips, &ports, id, listen_port);

    Vector_Destruct(&ports);
    Vector_Destruct(&ips);
}

static void Cleanup(void)
{
    Gossip_Terminate();
    Memory_Destroy();
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

    StartMemory();
    StartGossip();

    MainLoop();

    Cleanup();
}
