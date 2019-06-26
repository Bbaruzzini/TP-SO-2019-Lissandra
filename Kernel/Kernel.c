
#include "Config.h"
#include "Commands.h"
#include "Criteria.h"
#include "Metadata.h"
#include "Runner.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <Console.h>
#include <Defines.h>
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
#include <stdio.h>
#include <stdlib.h>
#include <Timer.h>

char const* CLIPrompt = "KRNL_LISSANDRA> ";

atomic_bool ProcessRunning = true;

KernelConfig ConfigKernel = { 0 };

static Appender* consoleLog = NULL;
static Appender* fileLog = NULL;

static PeriodicTimer* DescribeTimer = NULL;
static void PeriodicDescribe(void);

// feo copypaste de Memoria/Gossip.c pero we
typedef struct
{
    uint32_t MemId;
    char IP[INET6_ADDRSTRLEN];
    char Port[PORT_STRLEN];
} GossipPeer;
static t_dictionary* GossipPeers = NULL;

static inline void _addToIPPortDict(t_dictionary* dict, uint32_t memId, char const* IP, char const* Port)
{
    // suma + 1 de STR_LEN y + 1 de PORT_STRLEN pero está bien porque agrego el ':'
    char key[INET6_ADDRSTRLEN + PORT_STRLEN];
    snprintf(key, INET6_ADDRSTRLEN + PORT_STRLEN, "%s:%s", IP, Port);
    GossipPeer* gp = dictionary_get(dict, key);
    if (!gp)
        gp = Malloc(sizeof(GossipPeer));

    gp->MemId = memId;
    snprintf(gp->IP, INET6_ADDRSTRLEN, "%s", IP);
    snprintf(gp->Port, PORT_STRLEN, "%s", Port);

    dictionary_put(dict, key, gp);
}
static void DiscoverMemories(void);

static void IniciarLogger(void)
{
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, LMAGENTA, LCYAN, WHITE, YELLOW, LRED, RED);
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
    ConfigKernel.METADATA_REFRESH = config_get_long_value(config, "METADATA_REFRESH");
    atomic_store(&ConfigKernel.SLEEP_EJECUCION, config_get_long_value(config, "SLEEP_EJECUCION"));
    atomic_store(&ConfigKernel.QUANTUM, config_get_long_value(config, "QUANTUM"));

    config_destroy(config);

    // recargo el intervalo de metadata
    PeriodicTimer_ReSetTimer(DescribeTimer, ConfigKernel.METADATA_REFRESH);
}

static void SetupConfigInitial(char const* fileName)
{
    static uint32_t const METRICS_INTERVAL = 30 * 1000;
    static uint32_t const DISCOVERY_INTERVAL = 10 * 1000;

    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);

    t_config* config = config_create(fileName);
    if (!config)
    {
        LISSANDRA_LOG_FATAL("No se pudo abrir archivo de configuracion %s!", fileName);
        exit(EXIT_FAILURE);
    }

    snprintf(ConfigKernel.IP_MEMORIA, INET6_ADDRSTRLEN, "%s", config_get_string_value(config, "IP_MEMORIA"));
    snprintf(ConfigKernel.PUERTO_MEMORIA, PORT_STRLEN, "%s", config_get_string_value(config, "PUERTO_MEMORIA"));
    ConfigKernel.MULTIPROCESAMIENTO = config_get_long_value(config, "MULTIPROCESAMIENTO");

    ConfigKernel.METADATA_REFRESH = config_get_long_value(config, "METADATA_REFRESH");
    ConfigKernel.SLEEP_EJECUCION = config_get_long_value(config, "SLEEP_EJECUCION");
    ConfigKernel.QUANTUM = config_get_long_value(config, "QUANTUM");

    config_destroy(config);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, fileName, _reLoadConfig);
    EventDispatcher_AddFDI(fw);

    // timers
    DescribeTimer = PeriodicTimer_Create(ConfigKernel.METADATA_REFRESH, PeriodicDescribe);
    EventDispatcher_AddFDI(DescribeTimer);

    // cada 30 segundos debe mostrar las metricas por consola
    EventDispatcher_AddFDI(PeriodicTimer_Create(METRICS_INTERVAL, Criterias_Report));

    // cada 10 segundos? no dice nada el enunciado asi que pongo arbitrariamente el intervalo
    // en fin, cada 10 segundos dije! se hace el discovery de memorias aka gossiping
    EventDispatcher_AddFDI(PeriodicTimer_Create(DISCOVERY_INTERVAL, DiscoverMemories));
}

static void InitMemorySubsystem(void)
{
    Criterias_Init();
    Metadata_Init();

    GossipPeers = dictionary_create();

    DiscoverMemories();
}

static void MainLoop(void)
{
    // Runner cambia el manejador de consola asi que ejecuto antes de abrir la consola
    Runner_Init();

    // el kokoro
    pthread_t consoleTid;
    pthread_create(&consoleTid, NULL, CliThread, NULL);

    EventDispatcher_Loop();

    pthread_join(consoleTid, NULL);
}

static void Cleanup(void)
{
    dictionary_destroy_and_destroy_elements(GossipPeers, Free);
    Metadata_Destroy();
    Criterias_Destroy();
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

    InitMemorySubsystem();

    MainLoop();

    Cleanup();
}

static void PeriodicDescribe(void)
{
    // hago un describe global
    Vector args = string_n_split("DESCRIBE", 1, NULL);

    HandleDescribe(&args);

    Vector_Destruct(&args);
}

static void _iteratePeer(char const* _, void* val, void* diff)
{
    (void) _;
    GossipPeer* const gp = val;

    SocketOpts const so =
    {
        .HostName = gp->IP,
        .ServiceOrPort = gp->Port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    Socket* s = Socket_Create(&so);
    if (!s)
    {
        LISSANDRA_LOG_ERROR("DISCOVER: No pude conectarme al par gossip (%s:%s)!!", gp->IP, gp->Port);
        return;
    }

    static uint8_t const id = KERNEL;
    Packet* p = Packet_Create(MSG_HANDSHAKE, 20);
    Packet_Append(p, id);
    Packet_Append(p, gp->IP);
    Socket_SendPacket(s, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(s);
    if (!p)
    {
        LISSANDRA_LOG_ERROR("DISCOVER: Memoria (%s:%s) se desconectó durante handshake inicial!", gp->IP, gp->Port);
        Socket_Destroy(s);
        return;
    }

    if (Packet_GetOpcode(p) != MSG_GOSSIP_LIST)
    {
        LISSANDRA_LOG_ERROR("DISCOVER: Memoria (%s:%s) envió respuesta inválida %hu.", gp->IP, gp->Port, Packet_GetOpcode(p));
        Packet_Destroy(p);
        Socket_Destroy(s);
        return;
    }

    uint32_t numMems;
    Packet_Read(p, &numMems);
    for (uint32_t i = 0; i < numMems; ++i)
    {
        uint32_t memId;
        Packet_Read(p, &memId);

        char* memIp;
        Packet_Read(p, &memIp);

        char* memPort;
        Packet_Read(p, &memPort);

        _addToIPPortDict(diff, memId, memIp, memPort);

        Free(memPort);
        Free(memIp);
    }

    Packet_Destroy(p);
    Socket_Destroy(s);
}

static void _compareEntry(char const* key, void* val)
{
    GossipPeer* const gp = val;

    GossipPeer* my = dictionary_get(GossipPeers, key);
    if (my && my->MemId /* la seed no cuenta */)
    {
        // alguna diferencia?
        if (my->MemId == gp->MemId)
            return;

        LISSANDRA_LOG_INFO("DISCOVER: Descubierto nuevos datos para memoria en %s:%s! (MemId: %u, old: %u)", gp->IP,
                           gp->Port, gp->MemId, my->MemId);

        Criteria_DisconnectMemory(my->MemId);
    }
    else
        LISSANDRA_LOG_INFO("DISCOVER: Descubierta nueva memoria en %s:%s! (MemId: %u)", gp->IP, gp->Port, gp->MemId);

    Criteria_ConnectMemory(gp->MemId, gp->IP, gp->Port);
}

static void _disconnectOldMemories(char const* key, void* val, void* diff)
{
    GossipPeer* const gp = val;
    if (!dictionary_has_key(diff, key) && gp->MemId /* la seed no cuenta */)
    {
        LISSANDRA_LOG_INFO("DISCOVER: Memoria id %u (%s:%s) se desconecto!", gp->MemId, gp->IP, gp->Port);
        Criteria_DisconnectMemory(gp->MemId);
    }
}

static void DiscoverMemories(void)
{
    {
        // si la seed no esta porque se desconecto o es el gossip inicial o whatever, la agregamos a manopla
        char seed[INET6_ADDRSTRLEN + PORT_STRLEN];
        snprintf(seed, INET6_ADDRSTRLEN + PORT_STRLEN, "%s:%s", ConfigKernel.IP_MEMORIA, ConfigKernel.PUERTO_MEMORIA);
        if (!dictionary_has_key(GossipPeers, seed))
            _addToIPPortDict(GossipPeers, 0, ConfigKernel.IP_MEMORIA, ConfigKernel.PUERTO_MEMORIA);
    }

    t_dictionary* diff = dictionary_create();

    // descubre nuevas memorias y las agrego a diff
    dictionary_iterator_with_data(GossipPeers, _iteratePeer, diff);

    // quita las que tengo y no aparecen en el nuevo diff
    dictionary_iterator_with_data(GossipPeers, _disconnectOldMemories, diff);

    // agrega nuevas y actualiza datos
    dictionary_iterator(diff, _compareEntry);

    // por ultimo el diccionario queda actualizado con los nuevos items
    dictionary_destroy_and_destroy_elements(GossipPeers, Free);
    GossipPeers = diff;
}
