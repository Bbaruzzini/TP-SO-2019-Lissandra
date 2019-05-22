

#include "Lissandra.h"


CLICommand const CLICommands[] =
{
    { "SELECT",   HandleSelect   },
    { "INSERT",   HandleInsert   },
    { "CREATE",   HandleCreate   },
    { "DESCRIBE", HandleDescribe },
    { "DROP",     HandleDrop     },
    { NULL,       NULL           }
};

char const* CLIPrompt = "FS_LISSANDRA> ";

LockedQueue* CLICommandQueue = NULL;

atomic_bool ProcessRunning = true;

static Appender* consoleLog;
static Appender* fileLog;

static void IniciarLogger(void)
{
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    //Cambiamos el color "198EDC por "EA899A""
    consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, "EA899A");
    Logger_AddAppender(consoleLog);

    AppenderFlags const fileFlags = consoleFlags | APPENDER_FLAGS_USE_TIMESTAMP | APPENDER_FLAGS_MAKE_FILE_BACKUP;
    fileLog = AppenderFile_Create(LOG_LEVEL_ERROR, fileFlags, "lfs.log", "w", 0);
    Logger_AddAppender(fileLog);
}

//Era "static void" y lo cambiamos por "void*"
static void LoadConfig(char const* fileName)
{
    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);
    if (sConfig)
        config_destroy(sConfig);

    printf("PASO POR ACA\n");

    //Esto estaba
    sConfig = config_create(fileName);

    //Esto agregamos nosotras
    confLFS->PUERTO_ESCUCHA = string_duplicate(config_get_string_value(sConfig, "PUERTO_ESCUCHA"));

    printf("configPuerto %d\n",confLFS->PUERTO_ESCUCHA); //era para probar por consola, NO LO SAQUEN

    confLFS->PUNTO_MONTAJE = string_duplicate(config_get_string_value(sConfig,"PUNTO_MONTAJE"));
    if(!string_ends_with(confLFS->PUNTO_MONTAJE, "/"))
        string_append(&confLFS->PUNTO_MONTAJE, "/");

    printf("configTamValue %s\n",confLFS->PUNTO_MONTAJE); //era para probar por consola, NO LO SAQUEN

    confLFS->RETARDO = config_get_int_value(sConfig,"RETARDO");
    printf("configRetardo %d\n",confLFS->RETARDO);

    confLFS->TAMANIO_VALUE = config_get_int_value(sConfig,"TAMANIO_VALUE");
    printf("VALUE %d\n",confLFS->TAMANIO_VALUE);

    confLFS->TIEMPO_DUMP = config_get_int_value(sConfig,"TIEMPO_DUMP");
    printf("DUMP %d\n",confLFS->TIEMPO_DUMP);

    confLFS->TAMANIO_BLOQUES = config_get_int_value(sConfig,"TAMANIO_BLOQUES");
    printf("BLOQUES %d\n",confLFS->TAMANIO_BLOQUES);

    confLFS->CANTIDAD_BLOQUES = config_get_int_value(sConfig,"CANTIDAD_BLOQUES");
    printf("CANTBLOQUES %d\n",confLFS->CANTIDAD_BLOQUES);

    printf("ESTOY ACAAAAAA\n"); //era para probar por consola, NO LO SAQUEN

    LISSANDRA_LOG_TRACE("Config LFS iniciado");
    //printf("configTamValue %d\n",confLFS->TAMANIO_VALUE); //era para probar por consola, NO LO SAQUEN
    config_destroy(sConfig);
    sConfig = NULL;
}


int main(void)
{
    static char const* const configFileName = "lissandra.conf";

    IniciarLogger();

    //Esto agregamos nosotras
    confLFS = Malloc(sizeof(t_config_FS));

    printf("Llego hasta aca1\n");

    LoadConfig(configFileName);

    EventDispatcher_Init();

    // esta cola guarda los comandos que se nos envian por consola
    CLICommandQueue = LockedQueue_Create();

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, configFileName, LoadConfig);
    EventDispatcher_AddFDI(fw);

    printf("Llego hasta aca");

    iniciarMetadata();

    iniciar_servidor();

    //armar una funcion para que esto quede por fuera y el main mas limpio
    // limpieza
    LockedQueue_Destroy(CLICommandQueue, Free);
    Free(confLFS->PUNTO_MONTAJE);
    Free(confLFS->PUERTO_ESCUCHA);
    Free(confLFS);
    EventDispatcher_Terminate();
}
