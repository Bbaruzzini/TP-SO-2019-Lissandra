
#ifndef LISSANDRA_LISSANDRA_H
#define LISSANDRA_LISSANDRA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include "Appender.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "Console.h"
#include <Config.h>
#include <Console.h>
#include "API.h"
#include "EventDispatcher.h"
#include "FileWatcher.h"
#include "Logger.h"
#include "API.h"
#include "LissandraLibrary.h"
#include "FileSystem.h"

#include <signal.h>


typedef struct{
    char* PUERTO_ESCUCHA;
    char* PUNTO_MONTAJE;
    int RETARDO;
    int TAMANIO_VALUE;
    int TIEMPO_DUMP;
    int TAMANIO_BLOQUES;
    int CANTIDAD_BLOQUES;

}t_config_FS;

t_config_FS * confLFS;

#endif //LISSANDRA_LISSANDRA_H
