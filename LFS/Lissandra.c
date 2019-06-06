
#include "Lissandra.h"
#include "API.h"



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

    AppenderFlags const fileFlags = consoleFlags;
    fileLog = AppenderFile_Create(LOG_LEVEL_TRACE, fileFlags, "lfs.log", NULL, 0);
    Logger_AddAppender(fileLog);

}

//Era "static void" y lo cambiamos por "void*"
static void LoadConfig(char const* fileName)
{
    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);
    if (sConfig)
        config_destroy(sConfig);

    //Esto estaba
    sConfig = config_create(fileName);

    //Esto agregamos nosotras
    confLFS->PUERTO_ESCUCHA = string_duplicate(config_get_string_value(sConfig, "PUERTO_ESCUCHA"));

    confLFS->PUNTO_MONTAJE = string_duplicate(config_get_string_value(sConfig,"PUNTO_MONTAJE"));
    if(!string_ends_with(confLFS->PUNTO_MONTAJE, "/"))
        string_append(&confLFS->PUNTO_MONTAJE, "/");

    //printf("configTamValue %s\n",confLFS->PUNTO_MONTAJE); //era para probar por consola, NO LO SAQUEN

    confLFS->RETARDO = config_get_int_value(sConfig,"RETARDO");

    confLFS->TAMANIO_VALUE = config_get_int_value(sConfig,"TAMANIO_VALUE");

    confLFS->TIEMPO_DUMP = config_get_int_value(sConfig,"TIEMPO_DUMP");

    confLFS->TAMANIO_BLOQUES = config_get_int_value(sConfig,"TAMANIO_BLOQUES");

    confLFS->CANTIDAD_BLOQUES = config_get_int_value(sConfig,"CANTIDAD_BLOQUES");


    LISSANDRA_LOG_TRACE("Config LFS iniciado");
    //printf("configTamValue %d\n",confLFS->TAMANIO_VALUE); //era para probar por consola, NO LO SAQUEN
    config_destroy(sConfig);
    sConfig = NULL;
}


static void InitConsole(void)
{
    CLICommandQueue = LockedQueue_Create();

    // subimos el nivel a errores para no entorpecer la consola
    //Appender_SetLogLevel(consoleLog, LOG_LEVEL_ERROR);
}

static void pruebaConsola(void)
{
    // el kokoro
    printf("Llego a esta prueba\n");
    pthread_t consoleTid;
    pthread_create(&consoleTid, NULL, CliThread, NULL);

    EventDispatcher_Loop();

    pthread_join(consoleTid, NULL);
}



int main(void)
{
    static char const configFileName[] = "lissandra.conf";

    pthread_t hiloIniciarServidor;

    IniciarLogger();

    //Esto agregamos nosotras, esta aca xq sino en su lugar original genera un memory leak
    confLFS = Malloc(sizeof(t_config_FS));

    LoadConfig(configFileName);

    InitConsole();

    EventDispatcher_Init();

    // esta cola guarda los comandos que se nos envian por consola
    CLICommandQueue = LockedQueue_Create();

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, configFileName, LoadConfig);
    EventDispatcher_AddFDI(fw);

    //Aca va consola ->Update: La consola subio para aca
    //pruebaConsola();

    //pthread_create(&hiloIniciarServidor,NULL,(void*)&iniciar_servidor,NULL);

    iniciarFileSystem();

    //Pruebas Brenda/Denise desde ACA

    //create("Tabla3", "SC", 5, 2000);

   // t_list* prueba = malloc(sizeof(t_list));

   // prueba = describe("");

    //size_t tamanio = list_size(prueba);

   // printf("tamanio: %d\n", tamanio);

    //HASTA ACA

    iniciar_servidor();


    //TODO: armar una funcion para que esto quede por fuera y el main mas limpio
    // limpieza
    LockedQueue_Destroy(CLICommandQueue, Free);
    Free(confLFS->PUNTO_MONTAJE);
    Free(confLFS->PUERTO_ESCUCHA);
    Free(confLFS);
    EventDispatcher_Terminate();
}
