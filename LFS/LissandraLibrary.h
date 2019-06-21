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
#include <dirent.h>
#include <Packet.h>
#include <Opcodes.h>
#include "Handlers.h"


typedef struct
{
    char table[NAME_MAX + 1];
    uint8_t consistency;
    int partitions;
    int compaction_time;
} t_describe;

void iniciar_servidor(void);

void mkdirRecursivo(char const* path);

bool existeArchivo(char const* path);

bool existeDir(char const* pathDir);

void generarPathTabla(char const* nombreTabla, char* buf);

int buscarBloqueLibre(void);

void escribirValorBitarray(bool valor, int pos);

t_describe* get_table_metadata(char const* path, char const* tabla);

int traverse(char const* fn, t_list* lista, char const* tabla);

bool dirIsEmpty(char const* path);

bool hayDump(char const* nombreTabla);

bool is_any(char const* nombreArchivo);

char* generarPathArchivo(char const* nombreTabla, char const* nombreArchivo);

void borrarArchivo(char const* nombreTabla, char const* nombreArchivo);

int traverse_to_drop(char const* fn, char const* nombreTabla);

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//ATENCION!!!!!!!!!!!!! BRENDAAAAA DENISEEEEE --> t_pedido hay que armarlo nosotras!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO: dejo hecho un typedef solo para que compile
typedef struct pedido t_pedido;

t_pedido* obtener_pedido(void);

#endif //LISSANDRA_LISSANDRALIBRARY_H
