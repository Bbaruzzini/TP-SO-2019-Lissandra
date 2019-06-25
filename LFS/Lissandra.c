
#include "Lissandra.h"
#include "API.h"
#include "CLIHandlers.h"
#include "Memtable.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <Config.h>
#include <EventDispatcher.h>
#include <FileWatcher.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdio.h>

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

atomic_bool ProcessRunning = true;

static Appender* consoleLog;
static Appender* fileLog;

t_config_FS* confLFS = NULL;

static void IniciarLogger(void)
{
    Logger_Init(LOG_LEVEL_TRACE);

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    //Cambiamos el color "198EDC por "EA899A""
    consoleLog = AppenderConsole_Create(LOG_LEVEL_TRACE, consoleFlags, LGREEN, LRED, LRED, YELLOW, LGREEN, WHITE);
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

    confLFS->TAMANIO_BLOQUES = config_get_int_value(sConfig,"BLOCK_SIZE");

    confLFS->CANTIDAD_BLOQUES = config_get_int_value(sConfig,"BLOCKS");


    LISSANDRA_LOG_TRACE("Config LFS iniciado");
    //printf("configTamValue %d\n",confLFS->TAMANIO_VALUE); //era para probar por consola, NO LO SAQUEN
    config_destroy(sConfig);
    sConfig = NULL;
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

    //pthread_t hiloIniciarServidor;

    IniciarLogger();

    //Esto agregamos nosotras, esta aca xq sino en su lugar original genera un memory leak
    confLFS = Malloc(sizeof(t_config_FS));

    LoadConfig(configFileName);

    EventDispatcher_Init();

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, configFileName, LoadConfig);
    EventDispatcher_AddFDI(fw);

    //pthread_create(&hiloIniciarServidor,NULL,(void*)&iniciar_servidor,NULL);

    iniciarFileSystem();

    crearMemtable();
/*
 * Estas de aca son pruebas para ver si las funciones de la memtable andan
 *
    t_elem_memtable* elementoA = new_elem_memtable("TABLA1");

    t_registro* registro1 = new_elem_registro(3, "\"Este es el primer registro que pruebo\"", 1548421507);
    t_registro* registro2 = new_elem_registro(2, "\"Este es el segundo registro que pruebo\"", 1348451807);
    t_registro* registro3 = new_elem_registro(3, "\"Este es el tercer registro que pruebo\"", 1548421508);

    printf("Elementos del registro1: %d, %s, %d\n", registro1->key, registro1->value, registro1->timestamp);
    printf("Elementos del registro2: %d, %s, %d\n", registro2->key, registro2->value, registro2->timestamp);
    printf("Elementos del registro1: %d, %s, %d\n", registro3->key, registro3->value, registro3->timestamp);

    insert_new_in_memtable(elementoA);

    insert_new_in_registros("TABLA1", registro1);
    insert_new_in_registros("TABLA1", registro2);
    insert_new_in_registros("TABLA1", registro3);

    t_elem_memtable* new = memtable_get("TABLA1");

    printf("Este es el nombre de la tabla: %s\n", new->nombreTabla);

    size_t cantElementos = Vector_size(&new->registros);

    printf("La cantidad de elementos del elemento es: %d\n", cantElementos);

    t_registro* registro = registro_get_biggest_timestamp(new, 3);

    if(registro == NULL){
        printf("Esta vacio!!!\n");
    } else {
        printf("Este es el mayor timestamp: %d\n", registro->timestamp);
    }
*/
    //Pruebas Brenda/Denise desde ACA

    //api_create("Tabla3", "SC", 5, 2000);
    //api_create("Tabla2", "SC", 4, 1000);

    //t_list* prueba = malloc(sizeof(t_list));

    //prueba = api_describe("");

    //size_t tamanio = list_size(prueba);

    //printf("tamanio: %d\n", tamanio);

    //t_describe* prueba2 = Malloc(sizeof(t_describe));

    //prueba2 = api_describe("TABLA2");

    //printf("Tabla: %s\n", prueba2->table);
    //printf("Consistencia: %d\n", prueba2->consistency);
    //printf("Particiones: %d\n", prueba2->partitions);
    //printf("Tiempo: %d\n", prueba2->compaction_time);

    //HASTA ACA

    //api_drop("TABLA3");

    //printf("hora ariel: %d\n", GetMSEpoch());
    //printf("hora denise: %d\n", time(NULL));

    iniciar_servidor();

    //Aca va consola ->Update: La consola subio para aca
    pruebaConsola();

    //TODO: armar una funcion para que esto quede por fuera y el main mas limpio
    // limpieza
    Free(confLFS->PUNTO_MONTAJE);
    Free(confLFS->PUERTO_ESCUCHA);
    Free(confLFS);
    EventDispatcher_Terminate();
}
