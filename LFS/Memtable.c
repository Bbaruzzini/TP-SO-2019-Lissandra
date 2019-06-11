//
// Created by Denise on 07/06/2019.
//

#include "Memtable.h"

void crearMemtable()
{
    size_t memtableElementSize = sizeof(t_elem_memtable);
    Vector_Construct(&memtable, memtableElementSize, NULL, 0);
    LISSANDRA_LOG_TRACE("Memtable creada");
}

t_elem_memtable* new_elem_memtable(char* nombreTabla)
{
    t_elem_memtable* new = Malloc(sizeof(t_elem_memtable));
    new->nombreTabla = nombreTabla;
    size_t registrosSize = sizeof(t_registro);
    Vector_Construct(&new->registros, registrosSize, NULL, 0);

    return new;
}

t_registro* new_elem_registro(uint16_t key, char* value, time_t timestamp)
{
    t_registro* new = Malloc(sizeof(t_registro));
    new->key = key;
    new->value = value;
    new->timestamp = timestamp;

    return new;
}

void insert_new_in_memtable(t_elem_memtable* elemento)
{
    Vector_push_back(&memtable, elemento);
}

void insert_new_in_registros(char* nombreTabla, t_registro* registro)
{

    size_t cantElementos = Vector_size(&memtable);
    t_elem_memtable* elemento;
    int i = 0;

    while (i < cantElementos)
    {
        elemento = Vector_at(&memtable, i);
        //Aca puede que falle esta comparacion del if...
        if (elemento->nombreTabla == nombreTabla)
        {
            Vector_push_back(&elemento->registros, registro);
            return;
        }
        i++;
    }

    LISSANDRA_LOG_ERROR("No existe la tabla en la memtable");

}

t_elem_memtable* memtable_get(char* nombreTabla)
{
    size_t cantElementos = Vector_size(&memtable);
    t_elem_memtable* elemento;
    int i = 0;

    while (i < cantElementos)
    {
        elemento = Vector_at(&memtable, i);

        if (elemento->nombreTabla == nombreTabla)
        {
            return elemento;
        }
        i++;
    }

    elemento = NULL;

    return elemento;
}

t_registro* registro_get_biggest_timestamp(t_elem_memtable* elemento, uint16_t key)
{
    size_t cantElementos = Vector_size(&elemento->registros);
    t_registro* registro;
    t_registro* registroMayor = NULL;
    int timestamp = 0;
    int i = 0;

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
        i++;
    }

    return registroMayor;

}
