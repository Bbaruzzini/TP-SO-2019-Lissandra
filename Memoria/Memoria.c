
#include "API.h"
#include "Config.h"
#include "CLIHandlers.h"
#include "FileSystemSocket.h"
#include "Gossip.h"
#include "MainMemory.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <EventDispatcher.h>
#include <FileWatcher.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
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

MemConfig ConfigMemoria = { 0 };

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

static void _reLoadConfig(char const* fileName)
{
    LISSANDRA_LOG_INFO("Configuracion modificada, recargando campos...");
    t_config* config = config_create(fileName);
    if (!config)
    {
        LISSANDRA_LOG_ERROR("No se pudo abrir archivo de configuracion %s!", fileName);
        return;
    }

    // solo los campos recargables en tiempo ejecucion
    ConfigMemoria.RETARDO_MEM = config_get_long_value(config, "RETARDO_MEM");
    ConfigMemoria.RETARDO_FS = config_get_long_value(config, "RETARDO_FS");
    ConfigMemoria.RETARDO_JOURNAL = config_get_long_value(config, "RETARDO_JOURNAL");
    ConfigMemoria.RETARDO_GOSSIPING = config_get_long_value(config, "RETARDO_GOSSIPING");

    config_destroy(config);

    // recargar los timers
    PeriodicTimer_ReSetTimer(JournalTimer, ConfigMemoria.RETARDO_JOURNAL);
    PeriodicTimer_ReSetTimer(GossipTimer, ConfigMemoria.RETARDO_GOSSIPING);
}

static void SetupConfigInitial(char const* fileName)
{
    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);

    t_config* config = config_create(fileName);
    if (!config)
    {
        LISSANDRA_LOG_FATAL("No se pudo abrir archivo de configuracion %s!", fileName);
        exit(EXIT_FAILURE);
    }

    snprintf(ConfigMemoria.PUERTO, PORT_STRLEN, "%s", config_get_string_value(config, "PUERTO"));
    snprintf(ConfigMemoria.IP_FS, INET6_ADDRSTRLEN, "%s", config_get_string_value(config, "IP_FS"));
    snprintf(ConfigMemoria.PUERTO_FS, PORT_STRLEN, "%s", config_get_string_value(config, "PUERTO_FS"));
    ConfigMemoria.IP_SEEDS = config_get_array_value(config, "IP_SEEDS");
    ConfigMemoria.PUERTO_SEEDS = config_get_array_value(config, "PUERTO_SEEDS");
    ConfigMemoria.TAM_MEM = config_get_long_value(config, "TAM_MEM");
    ConfigMemoria.MEMORY_NUMBER = config_get_long_value(config, "MEMORY_NUMBER");

    ConfigMemoria.RETARDO_MEM = config_get_long_value(config, "RETARDO_MEM");
    ConfigMemoria.RETARDO_FS = config_get_long_value(config, "RETARDO_FS");
    ConfigMemoria.RETARDO_JOURNAL = config_get_long_value(config, "RETARDO_JOURNAL");
    ConfigMemoria.RETARDO_GOSSIPING = config_get_long_value(config, "RETARDO_GOSSIPING");

    config_destroy(config);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, fileName, _reLoadConfig);
    EventDispatcher_AddFDI(fw);

    // timers
    JournalTimer = PeriodicTimer_Create(ConfigMemoria.RETARDO_JOURNAL, API_Journal);
    EventDispatcher_AddFDI(JournalTimer);

    GossipTimer = PeriodicTimer_Create(ConfigMemoria.RETARDO_GOSSIPING, Gossip_Do);
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
    SocketOpts const so =
    {
        .HostName = ConfigMemoria.IP_FS,
        .ServiceOrPort = ConfigMemoria.PUERTO_FS,
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

    SocketOpts const so =
    {
        .HostName = NULL,
        .ServiceOrPort = ConfigMemoria.PUERTO,
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

static void Cleanup(void)
{
    Vector_Destruct(&ConfigMemoria.IP_SEEDS);
    Vector_Destruct(&ConfigMemoria.PUERTO_SEEDS);

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
    Gossip_Init();

    MainLoop();

    Cleanup();
}
