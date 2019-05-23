
#ifndef LISSANDRA_FILESYSTEM_H
#define LISSANDRA_FILESYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <libcommons/bitarray.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include "Lissandra.h"
#include "LissandraLibrary.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <Logger.h>

/**
 * Variables
 */

char* pathBloques, *pathTablas, *pathMetadata, *pathMetadataFS, *pathMetadataBitarray;

t_bitarray* bitArray;

//typedef uint32_t t_num;

//Semaforo

//pthread_mutex_t solicitud_mutex;

//Funciones

void iniciarMetadata();


#endif //LISSANDRA_FILESYSTEM_H


