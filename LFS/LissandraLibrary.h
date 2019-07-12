//
// Created by Denise on 26/04/2019.
//

#ifndef LISSANDRA_LISSANDRALIBRARY_H
#define LISSANDRA_LISSANDRALIBRARY_H

#include <libcommons/list.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <Timer.h>
#include "Memtable.h"

typedef struct
{
    char table[NAME_MAX + 1];
    uint8_t consistency;
    uint16_t partitions;
    uint32_t compaction_time;
} t_describe;

void iniciar_servidor(void);

void mkdirRecursivo(char const* path);

bool existeArchivo(char const* path);

bool existeDir(char const* pathDir);

void generarPathTabla(char* nombreTabla, char* buf);

bool buscarBloqueLibre(size_t* bloqueLibre);

void generarPathBloque(size_t numBloque, char* buf);

void escribirValorBitarray(bool valor, size_t pos);

t_describe* get_table_metadata(char const* path, char const* tabla);

int traverse(char const* fn, t_list* lista, char const* tabla);

bool dirIsEmpty(char const* path);

bool is_any(char const* nombreArchivo);

void generarPathArchivo(char const* nombreTabla, char const* nombreArchivo, char* buf);

void borrarArchivo(char const* nombreTabla, char const* nombreArchivo);

int traverse_to_drop(char const* fn, char const* nombreTabla);

uint16_t get_particion(uint16_t particiones, uint16_t key);

void generarPathParticion(uint16_t particion, char* pathTabla, char* pathParticion);

t_registro* get_biggest_timestamp(char const* contenido, uint16_t key);

t_registro* scanParticion(char const* pathParticion, uint16_t key);

t_registro* temporales_get_biggest_timestamp(char const* pathTabla, uint16_t key);

t_registro* get_newest(t_registro* particion, t_registro* temporales, t_registro* memtable);

// primitivas FS
char* leerArchivoLFS(char const* path);

void escribirArchivoLFS(char const* path, void const* buf, size_t len);

void crearArchivoLFS(char const* path, size_t block);

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//ATENCION!!!!!!!!!!!!! BRENDAAAAA DENISEEEEE --> t_pedido hay que armarlo nosotras!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO: dejo hecho un typedef solo para que compile
typedef struct pedido t_pedido;

t_pedido* obtener_pedido(void);

#endif //LISSANDRA_LISSANDRALIBRARY_H
