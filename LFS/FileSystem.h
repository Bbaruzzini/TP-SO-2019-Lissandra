
#ifndef LISSANDRA_FILESYSTEM_H
#define LISSANDRA_FILESYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <commons/config.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <libcommons/bitarray.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include "Lissandra.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <Logger.h>

/**
 * Variables
 */

char* pathBloques, *pathTablas, *pathMetadata, *pathMetadataTabla, *pathMetadataBitarray;

t_bitarray* bitArray;

typedef uint32_t t_num;

//Semaforo

pthread_mutex_t solicitud_mutex;

//Funciones

void iniciarMetadata();
void mkdirRecursivo(char* path);

//Archivos

bool existeArchivo(char* path);


#endif //LISSANDRA_FILESYSTEM_H


