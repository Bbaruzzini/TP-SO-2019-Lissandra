//
// Created by Denise on 07/06/2019.
//

#ifndef LISSANDRA_MEMTABLE_H
#define LISSANDRA_MEMTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Malloc.h"
#include "Lissandra.h"
#include "Logger.h"
#include "vector.h"

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
    uint16_t key;
    char* value;
    time_t timestamp;
} t_registro;


void crearMemtable();

//Funcion para crear un nuevo elemento del tipo t_elem_memtable
t_elem_memtable* new_elem_memtable(char* nombreTabla);

//Funcion para crear nuevo elemento del tipo t_registro
t_registro* new_elem_registro(uint16_t key, char* value, time_t timestamp);

//Funcion para meterle nuevos elementos a la memtable
void insert_new_in_memtable(t_elem_memtable* elemento);

//Funcion para meterle nuevos elementos (registros) al vector registros de un elemento de la memtable
//Se necesita el nombre de la tabla al cual agregar el registro
void insert_new_in_registros(char* nombreTabla, t_registro* registro);

//Funcion para buscar, si existe, un elemento en la memTable para una cierta Tabla
//Si existe, lo retorna, sino retorna NULL
t_elem_memtable* memtable_get(char* nombreTabla);

//Funcion para buscar segun una key dada el registro con mayor timestamp
t_registro* registro_get_biggest_timestamp(t_elem_memtable* elemento, uint16_t key);


#endif //LISSANDRA_MEMTABLE_H
