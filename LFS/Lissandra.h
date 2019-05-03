//

//

#ifndef LISSANDRA_LISSANDRA_H
#define LISSANDRA_LISSANDRA_H

#include "Appender.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "Console.h"
#include <Config.h>
#include <Console.h>
#include "CLIHandlers.h"
#include "EventDispatcher.h"
#include "FileWatcher.h"
#include "Logger.h"
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>


typedef struct{
    int PUERTO_ESCUCHA;
    char* PUNTO_MONTAJE;
    int RETARDO;
    int TAMANIO_VALUE;
    int TIEMPO_DUMP;

}t_config_FS;




#endif //LISSANDRA_LISSANDRA_H
