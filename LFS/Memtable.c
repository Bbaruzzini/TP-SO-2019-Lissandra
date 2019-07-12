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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static t_dictionary* memtable = NULL;
static pthread_rwlock_t memtableMutex = PTHREAD_RWLOCK_INITIALIZER;

static void _delete_memtable_table(void* registros)
{
    Vector_Destruct(registros);
    Free(registros);
}

static void _delete_memtable_register(void* elem)
{
    t_registro* r = elem;
    Free(r->value);
}

void memtable_create(void)
{
    memtable = dictionary_create();
    LISSANDRA_LOG_TRACE("Memtable creada");
}

void memtable_new_elem(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp)
{
    t_registro new =
    {
        .key = key,
        .value = string_duplicate(value),
        .timestamp = timestamp
    };

    pthread_rwlock_wrlock(&memtableMutex);

    Vector* registros = dictionary_get(memtable, nombreTabla);
    if (!registros)
    {
        registros = Malloc(sizeof(Vector));
        Vector_Construct(registros, sizeof(t_registro), _delete_memtable_register, 0);

        dictionary_put(memtable, nombreTabla, registros);
    }

    Vector_push_back(registros, &new);

    pthread_rwlock_unlock(&memtableMutex);
}

bool memtable_has_table(char const* nombreTabla)
{
    bool res;
    pthread_rwlock_rdlock(&memtableMutex);
    res = dictionary_has_key(memtable, nombreTabla);
    pthread_rwlock_unlock(&memtableMutex);

    return res;
}

t_registro* memtable_get_biggest_timestamp(char const* nombreTabla, uint16_t key)
{
    pthread_rwlock_rdlock(&memtableMutex);

    Vector* const registros = dictionary_get(memtable, nombreTabla);

    size_t const cantElementos = Vector_size(registros);

    t_registro* registroMayor = NULL;
    uint64_t timestampMayor = 0;

    for (size_t i = 0; i < cantElementos; ++i)
    {
        t_registro* const registro = Vector_at(registros, i);
        if (registro->key == key)
        {
            if (!registroMayor || registro->timestamp > timestampMayor)
            {
                timestampMayor = registro->timestamp;
                registroMayor = registro;
            }
        }
    }

    t_registro* resultado = NULL;
    if (registroMayor)
    {
        resultado = Malloc(sizeof(t_registro));
        *resultado = *registroMayor;
        resultado->value = strdup(registroMayor->value);
    }

    pthread_rwlock_unlock(&memtableMutex);
    return resultado;
}

void memtable_delete_table(char const* nombreTabla)
{
    pthread_rwlock_wrlock(&memtableMutex);

    dictionary_remove_and_destroy(memtable, nombreTabla, _delete_memtable_table);

    pthread_rwlock_unlock(&memtableMutex);
}

static void _dump_element(void* element, void* path)
{
    t_registro* registro = element;
    char const* pathTemporal = path;

    size_t len = snprintf(NULL, 0, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);
    char field[len + 1];
    snprintf(field, len + 1, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);

    escribirArchivoLFS(pathTemporal, field, len);
}

static void _dump_table(char const* nombreTabla, void* registros)
{
    char pathTable[PATH_MAX];
    snprintf(pathTable, PATH_MAX, "%sTables/%s", confLFS.PUNTO_MONTAJE, nombreTabla);
    if (!existeDir(pathTable))
    {
        LISSANDRA_LOG_ERROR("La tabla hallada en la memtable no se encuentra en el File System... Esto no deberia estar pasando");
        return;
    }

    //Arma el path del archivo temporal. Si ese path ya existe, le busca nombre hasta encontrar uno libre
    char pathTemporal[PATH_MAX];
    for (size_t j = 0; snprintf(pathTemporal, PATH_MAX, "%sTables/%s/%d.tmp", confLFS.PUNTO_MONTAJE, nombreTabla, j), existeArchivo(pathTemporal); ++j);

    size_t bloqueLibre;
    if (!buscarBloqueLibre(&bloqueLibre))
    {
        LISSANDRA_LOG_ERROR("No hay espacio en el File System. Abortando...");
        exit(EXIT_FAILURE);
    }

    // todo crearArchivoVacio(pathTemporal, bloqueLibre);
    FILE* temporal = fopen(pathTemporal, "w");
    fprintf(temporal, "SIZE=0\n");
    fprintf(temporal, "BLOCKS=[%d]\n", bloqueLibre);
    fclose(temporal);

    Vector_iterate_with_data(registros, _dump_element, pathTemporal);
}

void memtable_dump(void)
{
    t_dictionary* oldMemtable;

    {
        //intercambio punteros
        pthread_rwlock_wrlock(&memtableMutex);
        oldMemtable = memtable;
        memtable = dictionary_create();
        pthread_rwlock_unlock(&memtableMutex);
    }

    //Itera tantas veces como tablas contó que había en ese momento en la memtable
    dictionary_iterator(oldMemtable, _dump_table);
    dictionary_destroy_and_destroy_elements(oldMemtable, _delete_memtable_table);
}
