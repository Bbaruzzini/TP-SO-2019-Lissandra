//
// Created by Denise on 26/04/2019.
//

#ifndef LISSANDRA_LISSANDRALIBRARY_H
#define LISSANDRA_LISSANDRALIBRARY_H

#include <libcommons/list.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

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

bool hayDump(char const* nombreTabla);

bool is_any(char const* nombreArchivo);

void generarPathArchivo(char const* nombreTabla, char const* nombreArchivo, char* buf);

void borrarArchivo(char const* nombreTabla, char const* nombreArchivo);

int traverse_to_drop(char const* fn, char const* nombreTabla);

// primitivas FS
void escribirArchivoLFS(char const* path, void const* buf, size_t len);

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//ATENCION!!!!!!!!!!!!! BRENDAAAAA DENISEEEEE --> t_pedido hay que armarlo nosotras!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO: dejo hecho un typedef solo para que compile
typedef struct pedido t_pedido;

t_pedido* obtener_pedido(void);

#endif //LISSANDRA_LISSANDRALIBRARY_H
