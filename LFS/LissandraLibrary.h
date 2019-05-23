//
// Created by Denise on 26/04/2019.
//

#ifndef LISSANDRA_LISSANDRALIBRARY_H
#define LISSANDRA_LISSANDRALIBRARY_H

#include <libcommons/list.h>
#include "Lissandra.h"
#include <LockedQueue.h>
#include <semaphore.h>
#include <Socket.h>

void iniciar_servidor(void);

void mkdirRecursivo(char* path);

bool existeArchivo(char* path);

bool existeDir(char* pathDir);

char* generarPathTabla(char* nombreTabla);

int buscarBloqueLibre();

void escribirValorBitarray(bool valor, int pos);



//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//ATENCION!!!!!!!!!!!!! BRENDAAAAA DENISEEEEE --> t_pedido hay que armarlo nosotras!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO: dejo hecho un typedef solo para que compile
typedef struct pedido t_pedido;

t_pedido* obtener_pedido(void);

#endif //LISSANDRA_LISSANDRALIBRARY_H
