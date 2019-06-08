//
// Created by Denise on 07/06/2019.
//

#ifndef LISSANDRA_MEMTABLE_H
#define LISSANDRA_MEMTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include "Malloc.h"
#include "Lissandra.h"

typedef struct
{
    char* nombreTabla;
    Vector registros;
} t_elem_memtable;

//Querido Ariel: si de casualidad pasas por aca y lees esto, te agradeceria de corazon si me podes decir
//si el key, value y timestamp son int, int32_t, int16_t o cual de todos los int, porque la verdad verdad,
//no tengo la menor idea :D Graciaaaaaaas!
typedef struct
{
    int key;
    int value;
    int timestamp;
} t_registro;


void crearMemtable();

t_elem_memtable* new_elem_memtable(char* nombreTabla);

t_registro* new_elem_registro(int key, int value, int timestamp);

void insert_new_in_memTable(t_elem_memtable* elemento);

void insert_new_in_registros(t_elem_memtable* elemento, t_registro* registro);

t_elem_memtable* memtable_get(char* nombreTabla);

t_registro* registro_get_biggest_timestamp(t_elem_memtable* elemento, int key);


#endif //LISSANDRA_MEMTABLE_H
