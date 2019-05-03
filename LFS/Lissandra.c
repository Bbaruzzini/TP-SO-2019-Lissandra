

#include "Lissandra.h"

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
    t_config_FS * conf = malloc(sizeof(t_config_FS));

    //Esto estaba
    sConfig = config_create(fileName);

    //Esto agregamos nosotras
    conf->PUERTO_ESCUCHA = config_get_int_value(sConfig,"PUERTO_ESCUCHA");

    conf->PUNTO_MONTAJE = malloc(strlen(config_get_string_value(sConfig,"PUNTO_MONTAJE"))+1);
    strcpy(conf->PUNTO_MONTAJE, config_get_string_value(sConfig,"PUNTO_MONTAJE"));
    if(!string_ends_with(conf->PUNTO_MONTAJE, "/")) string_append(&conf->PUNTO_MONTAJE, "/");

    conf->RETARDO = config_get_int_value(sConfig,"RETARDO");
    conf->TAMANIO_VALUE = config_get_int_value(sConfig,"TAMANIO_VALUE");
    conf->TIEMPO_DUMP = config_get_int_value(sConfig,"TIEMPO_DUMP");

    config_destroy(sConfig);

    return conf;

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



}
