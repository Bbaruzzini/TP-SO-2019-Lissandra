//
// Created by Denise on 07/06/2019.
//

#include "Memtable.h"
#include "LissandraLibrary.h"
#include <fcntl.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Malloc.h>
#include <libcommons/config.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Config.h"

static Vector memtable;

static void _delete_memtable_table(void* elem)
{
    t_elem_memtable* mt = elem;
    Free(mt->nombreTabla);
    Vector_Destruct(&mt->registros);
}

static void _delete_memtable_register(void* elem)
{
    t_registro* r = elem;
    Free(r->value);
}

void crearMemtable(void)
{
    size_t const memtableElementSize = sizeof(t_elem_memtable);
    Vector_Construct(&memtable, memtableElementSize, _delete_memtable_table, 0);
    LISSANDRA_LOG_TRACE("Memtable creada");
}

t_elem_memtable* new_elem_memtable(char const* nombreTabla)
{
    t_elem_memtable* new = Malloc(sizeof(t_elem_memtable));
    new->nombreTabla = string_duplicate(nombreTabla);
    size_t const registrosSize = sizeof(t_registro);
    Vector_Construct(&new->registros, registrosSize, _delete_memtable_register, 0);

    return new;
}

t_registro* new_elem_registro(uint16_t key, char const* value, uint64_t timestamp)
{
    t_registro* new = Malloc(sizeof(t_registro));
    new->key = key;
    new->value = string_duplicate(value);
    new->timestamp = timestamp;

    return new;
}

void insert_new_in_memtable(t_elem_memtable* elemento)
{
    Vector_push_back(&memtable, elemento);
}

void insert_new_in_registros(char const* nombreTabla, t_registro* registro)
{
    size_t cantElementos = Vector_size(&memtable);
    t_elem_memtable* elemento;
    size_t i = 0;

    while (i < cantElementos)
    {
        elemento = Vector_at(&memtable, i);

        if (0 == strcmp(elemento->nombreTabla, nombreTabla))
        {
            Vector_push_back(&elemento->registros, registro);
            return;
        }
        ++i;
    }

    LISSANDRA_LOG_ERROR("No existe la tabla en la memtable");
}

t_elem_memtable* memtable_get(char const* nombreTabla)
{
    size_t cantElementos = Vector_size(&memtable);
    t_elem_memtable* elemento;
    size_t i = 0;

    while (i < cantElementos)
    {
        elemento = Vector_at(&memtable, i);

        if (0 == strcmp(elemento->nombreTabla, nombreTabla))
            return elemento;

        ++i;
    }

    elemento = NULL;

    return elemento;
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

int delete_elem_memtable(char const* nombreTabla)
{
    size_t cantElementos = Vector_size(&memtable);
    t_elem_memtable* elemento;
    size_t i = 0;

    while (i < cantElementos)
    {
        elemento = Vector_at(&memtable, i);

        if (0 == strcmp(elemento->nombreTabla, nombreTabla))
        {
            Vector_erase(&memtable, i);
            return 0;
        }
        i++;
    }

    return -1;

}

void dump(){

    //Aca debería bloquear esto?
    size_t cantTablas = Vector_size(&memtable);
    //Hasta aca?
    t_elem_memtable* elemento;
    t_registro* registro;
    FILE* temporal;
    char* table;
    char pathTable[PATH_MAX];
    char pathBloque[PATH_MAX];
    char pathTemporal[PATH_MAX];
    char buffer[1024];

    //Itera tantas veces como tablas contó que había en ese momento en la memtable
    size_t bloqueLibre;
    size_t i = 0;
    while (i < cantTablas){

        elemento = Vector_at(&memtable, i);
        table = elemento->nombreTabla;
        snprintf(pathTable, PATH_MAX,  "%sTables/%s", confLFS.PUNTO_MONTAJE, table);
        if(!existeDir(pathTable))
        {
            LISSANDRA_LOG_ERROR("La tabla hallada en la memtable no se encuentra en el File System...Esto no deberia estar pasando");
            ++i;
            continue;
        }
        //Arma el path del archivo temporal. Si ese path ya existe, le busca nombre hasta encontrar uno libre
        size_t j = 0;
        snprintf(pathTemporal, PATH_MAX,  "%sTables/%s/%d.tmp", confLFS.PUNTO_MONTAJE, table, j);
        while(existeArchivo(pathTemporal))
        {
            ++j;
            snprintf(pathTemporal, PATH_MAX,  "%sTables/%s/%d.tmp", confLFS.PUNTO_MONTAJE, table, j);
        }

        if (!buscarBloqueLibre(&bloqueLibre))
        {
            LISSANDRA_LOG_ERROR("No hay espacio en el File System. Abortando dump");
            return;
        }
        escribirValorBitarray(true, bloqueLibre);

        temporal = fopen(pathTemporal, "a");
        fprintf(temporal, "SIZE=0\n");
        fprintf(temporal, "BLOCKS=[%d]\n", bloqueLibre);
        fclose(temporal);

        size_t espacioDisponible = confLFS.TAMANIO_BLOQUES;
        //Aca deberia bloquear esto?
        size_t cantRegistros = Vector_size(&elemento->registros);
        //Hasta aca?
        size_t k = 0;
        size_t sizeTotal = 0;
        size_t totalRegistros = 0;
        int offset = 0;
        int resEscritura;
        while(k < cantRegistros)
        {
            registro = Vector_at(&elemento->registros, k);
            snprintf(buffer, 1024, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);
            size_t tamanioRegistro = strlen(buffer);
            generarPathBloque(bloqueLibre, pathBloque);
            if(tamanioRegistro <= espacioDisponible)
            {
                //Escribir registro
                resEscritura = escribirBloque(pathBloque, tamanioRegistro, buffer, offset);
                if(resEscritura == EXIT_FAILURE){
                    LISSANDRA_LOG_ERROR("Se produjo un error al intentar escribir el bloque");
                    ++k;
                    continue;
                }
                espacioDisponible = espacioDisponible - tamanioRegistro;
            } else {
                //Tengo que agarrar un nuevo bloque libre, escribir lo que puedo del registro en lo que me quedo del otro bloque y el resto en el nuevo
                int cantBloques = (tamanioRegistro-espacioDisponible)/confLFS.TAMANIO_BLOQUES;
                if((tamanioRegistro-espacioDisponible)%confLFS.TAMANIO_BLOQUES != 0){
                    ++cantBloques;
                }
                //TODO:validar si hay suficientes bloques libres y si no hay abortar dump
                resEscritura = escribirBloque(pathBloque, espacioDisponible, buffer, offset); //el buffer tiene que tener solo la porcion que se puede escribir en ese espacio disponible
                if(resEscritura == EXIT_FAILURE){
                    LISSANDRA_LOG_ERROR("Se produjo un error al intentar escribir el bloque");
                    ++k;
                    continue;
                }
                snprintf(buffer, 1024, "%s", (buffer + espacioDisponible));
                espacioDisponible = confLFS.TAMANIO_BLOQUES;
                for(int a = 0; a < cantBloques; ++a){
                    bloqueLibre = reservarNuevoBloqueLibre(pathTemporal);
                    generarPathBloque(bloqueLibre, pathBloque);
                    if(strlen(buffer) > confLFS.TAMANIO_BLOQUES){
                        resEscritura = escribirBloque(pathBloque, confLFS.TAMANIO_BLOQUES, buffer, 0);
                        if(resEscritura == EXIT_FAILURE){
                            LISSANDRA_LOG_ERROR("Se produjo un error al intentar escribir el bloque");
                            continue;
                        }
                        snprintf(buffer, 1024, "%s", (buffer + espacioDisponible));
                    } else {
                        resEscritura = escribirBloque(pathBloque, strlen(buffer), buffer, 0);//el buffer tiene que tener solo la porcion que me falta escribir
                        if(resEscritura == EXIT_FAILURE){
                            LISSANDRA_LOG_ERROR("Se produjo un error al intentar escribir el bloque");
                            continue;
                        }
                        espacioDisponible = espacioDisponible - strlen(buffer);
                    }
                }
                offset = strlen(buffer) - tamanioRegistro;
            }
            offset = offset + tamanioRegistro ;
            sizeTotal = sizeTotal + tamanioRegistro;
            ++totalRegistros;
            ++k;
        }

        //Borrar de la memtable la cantidad de registros totalRegistros (para la tabla correspondiente)
        Vector_erase_range(&elemento->registros, 0, (totalRegistros));
        //Anotar en el .tmp el size total
        cambiarSize(pathTemporal, sizeTotal);

        ++i;
    }

}

int escribirBloque(char* pathBloque, size_t tamanioRegistro, char* buffer, int offset){
    int fd = open(pathBloque, O_RDWR | O_CREAT, S_IRWXU);
    if(fd == -1){
        LISSANDRA_LOG_ERROR("Se produjo un error al intentar abrir el archivo %s", pathBloque);
        return EXIT_FAILURE;
    }
    lseek(fd, (offset+tamanioRegistro-1), SEEK_SET);
    write(fd, "", 1);
    char* map = mmap(0, (offset + tamanioRegistro), PROT_READ | PROT_WRITE, MAP_SHARED, fd, SEEK_SET);
    if (map == MAP_FAILED) {
        close(fd);
        LISSANDRA_LOG_ERROR("Error haciendo el mapping");
        return EXIT_FAILURE;
    }
    strcat(map, buffer);
    //memcpy(map, buffer, tamanioRegistro);
    //strcpy(map + offset, buffer);
    msync(map, tamanioRegistro, MS_SYNC);
    munmap(map, tamanioRegistro);
    close(fd);
    return EXIT_SUCCESS;
}

void generarPathBloque(int numBloque, char* buf){
    snprintf(buf, PATH_MAX,  "%sBloques/%d.bin", confLFS.PUNTO_MONTAJE, numBloque);
}

//Revisar esta
size_t reservarNuevoBloqueLibre(char* pathArchivo){
    size_t bloqueLibre;
    if(!buscarBloqueLibre(&bloqueLibre))
        return false;

    escribirValorBitarray(true, bloqueLibre);

    t_config* c = config_create(pathArchivo);
    char bloques[1024];
    size_t length = strlen(config_get_string_value(c, "BLOCKS"))-1;
    snprintf(bloques, 1024, "%*.*s,%d]", length, length, config_get_string_value(c, "BLOCKS"), bloqueLibre);

    config_set_value(c,"BLOCKS", bloques);
    config_save(c);
    config_destroy(c);

    return bloqueLibre;
}

void cambiarSize(char* pathArchivo, size_t newSize){
    char newTamanio[10];
    snprintf(newTamanio, 10, "%d", newSize);
    t_config* c = config_create(pathArchivo);
    config_set_value(c, "SIZE", newTamanio);
    config_save(c);
    config_destroy(c);
}

