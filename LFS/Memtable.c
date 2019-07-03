//
// Created by Denise on 07/06/2019.
//

#include "Memtable.h"
#include "Config.h"
#include "LissandraLibrary.h"
#include <libcommons/config.h>
#include <libcommons/dictionary.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdio.h>

static t_dictionary* memtable = NULL;

static void _delete_memtable_table(void* elem)
{
    t_elem_memtable* mt = elem;
    Free(mt->nombreTabla);
    Vector_Destruct(&mt->registros);
    Free(mt);
}

static void _delete_memtable_register(void* elem)
{
    t_registro* r = elem;
    Free(r->value);
}

void crearMemtable(void)
{
    memtable = dictionary_create();
    LISSANDRA_LOG_TRACE("Memtable creada");
}

void new_elem_memtable(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp)
{
    t_registro new =
    {
        .key = key,
        .value = string_duplicate(value),
        .timestamp = timestamp
    };

    t_elem_memtable* table = dictionary_get(memtable, nombreTabla);
    if (!table)
    {
        table = Malloc(sizeof(t_elem_memtable));
        table->nombreTabla = string_duplicate(nombreTabla);
        Vector_Construct(&table->registros, sizeof(t_registro), _delete_memtable_register, 0);

        dictionary_put(memtable, nombreTabla, table);
    }

    Vector_push_back(&table->registros, &new);
}

t_elem_memtable* memtable_get(char const* nombreTabla)
{
    return dictionary_get(memtable, nombreTabla);
}

t_registro* registro_get_biggest_timestamp(t_elem_memtable* elemento, uint16_t key)
{
    size_t cantElementos = Vector_size(&elemento->registros);
    t_registro* registro;
    t_registro* registroMayor = NULL;
    uint64_t timestamp = 0;
    size_t i = 0;

    while (i < cantElementos)
    {
        registro = Vector_at(&elemento->registros, i);

        if (registro->key == key)
        {
            if (registro->timestamp > timestamp)
            {
                timestamp = registro->timestamp;
                registroMayor = registro;
            }
        }
        ++i;
    }

    return registroMayor;
}

void delete_elem_memtable(char const* nombreTabla)
{
    dictionary_remove_and_destroy(memtable, nombreTabla, _delete_memtable_table);
}

static void _dump_element(void* element, void* path)
{
    t_registro* registro = element;
    char const* pathTemporal = path;

    size_t len = snprintf(NULL, 0, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);
    char* field = Malloc(len + 1);
    snprintf(field, len + 1, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);

    escribirArchivoLFS(pathTemporal, field, len);

    Free(field);
}

static void _dump_table(char const* key, void* element)
{
    (void) key;

    t_elem_memtable* table = element;

    char pathTable[PATH_MAX];
    snprintf(pathTable, PATH_MAX, "%sTables/%s", confLFS.PUNTO_MONTAJE, table->nombreTabla);
    if (!existeDir(pathTable))
    {
        LISSANDRA_LOG_ERROR("La tabla hallada en la memtable no se encuentra en el File System... Esto no deberia estar pasando");
        return;
    }

    //Arma el path del archivo temporal. Si ese path ya existe, le busca nombre hasta encontrar uno libre
    char pathTemporal[PATH_MAX];
    for (size_t j = 0; snprintf(pathTemporal, PATH_MAX, "%sTables/%s/%d.tmp", confLFS.PUNTO_MONTAJE, table->nombreTabla, j), existeArchivo(pathTemporal); ++j);

    size_t bloqueLibre;
    if (!buscarBloqueLibre(&bloqueLibre))
    {
        LISSANDRA_LOG_ERROR("No hay espacio en el File System. Abortando dump");
        return;
    }

    // todo crearArchivoVacio(pathTemporal, bloqueLibre);
    FILE* temporal = fopen(pathTemporal, "w");
    fprintf(temporal, "SIZE=0\n");
    fprintf(temporal, "BLOCKS=[%d]\n", bloqueLibre);
    fclose(temporal);

    Vector_iterate_with_data(&table->registros, _dump_element, pathTemporal);

    //Borrar de la memtable los registros (para la tabla correspondiente)
    Vector_clear(&table->registros);
}

void dump(void)
{
    //Aca debería bloquear esto?
    //Hasta aca?

    //Itera tantas veces como tablas contó que había en ese momento en la memtable
    dictionary_iterator(memtable, _dump_table);
}
