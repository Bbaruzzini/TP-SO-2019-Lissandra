

#include "Lissandra.h"
#include "LissandraLibrary.h"

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

    //Esto agregamos nosotras
    confLFS = malloc(sizeof(t_config_FS));

    //Esto estaba
    sConfig = config_create(fileName);

    //Esto agregamos nosotras
    confLFS->PUERTO_ESCUCHA = config_get_int_value(sConfig,"PUERTO_ESCUCHA");

    confLFS->PUNTO_MONTAJE = malloc(strlen(config_get_string_value(sConfig,"PUNTO_MONTAJE"))+1);
    strcpy(confLFS->PUNTO_MONTAJE, config_get_string_value(sConfig,"PUNTO_MONTAJE"));
    if(!string_ends_with(confLFS->PUNTO_MONTAJE, "/")) string_append(&confLFS->PUNTO_MONTAJE, "/");

    confLFS->RETARDO = config_get_int_value(sConfig,"RETARDO");
    confLFS->TAMANIO_VALUE = config_get_int_value(sConfig,"TAMANIO_VALUE");
    confLFS->TIEMPO_DUMP = config_get_int_value(sConfig,"TIEMPO_DUMP");

    config_destroy(sConfig);

}


int main(void)
{

    static char const* const configFileName = "lissandra.conf";
    IniciarLogger();
    LoadConfig(configFileName);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, configFileName, LoadConfig);
    EventDispatcher_AddFDI(fw);

    iniciar_servidor();

}
