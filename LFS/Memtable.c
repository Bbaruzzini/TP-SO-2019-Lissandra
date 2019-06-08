//
// Created by Denise on 07/06/2019.
//

#include "Memtable.h"

//Dejo otro comentario para Ariel: Si llegas a ver esto antes de que probemos si estas funciones andan,
//aclaro que no probamos nada todavia xD No nos juzgues!!!!!

void crearMemtable()
{
    size_t memTableElementSize = sizeof(t_elem_memtable);
    Vector_Construct(&memTable, memTableElementSize, NULL, 0);
}

//Funcion para crear un nuevo elemento del tipo t_elem_memtable
t_elem_memtable* new_elem_memtable(char* nombreTabla)
{
    t_elem_memtable* new = Malloc(sizeof(t_elem_memtable));
    new->nombreTabla = nombreTabla;
    size_t registrosSize = sizeof(t_registro);
    Vector_Construct(&new->registros, registrosSize, NULL, 0);

    return new;
}

//Funcion para crear nuevo elemento del tipo t_registro
t_registro* new_elem_registro(int key, int value, int timestamp)
{
    t_registro* new = Malloc(sizeof(t_registro));
    new->key = key;
    new->value = value;
    new->timestamp = timestamp;

    return new;
}

//Funcion para meterle nuevos elementos a la memTable
void insert_new_in_memTable(t_elem_memtable* elemento)
{
    Vector_push_back(&memTable, elemento);
}

//Funcion para meterle nuevos elementos (registros) al vector registros de un elemento de la memTable
void insert_new_in_registros(t_elem_memtable* elemento, t_registro* registro)
{
    Vector_push_back(&elemento->registros, registro);
}

//Funcion para buscar, si existe, un elemento en la memTable para una cierta Tabla
//Si existe, lo retorna, sino retorna NULL
t_elem_memtable* memtable_get(char* nombreTabla)
{
    size_t cantElementos = Vector_size(&memTable);
    t_elem_memtable* elemento;
    int i = 0;

    while (i < cantElementos)
    {
        elemento = Vector_at(&memTable, i);
        //Aca puede que falle esta comparacion del if...
        if (elemento->nombreTabla == nombreTabla)
        {
            return elemento;
        }
        i++;
    }

    elemento = NULL;

    return elemento;
}

//Funcion para buscar segun una key dada el registro con mayor timestamp
t_registro* registro_get_biggest_timestamp(t_elem_memtable* elemento, int key)
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
