//
// Created by Denise on 07/06/2019.
//

#include "Memtable.h"

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
        i++;
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
