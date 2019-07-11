//
// Created by Denise on 07/06/2019.
//

#ifndef LISSANDRA_MEMTABLE_H
#define LISSANDRA_MEMTABLE_H

#include <stdint.h>
#include <vector.h>

typedef struct
{
    uint16_t key;
    char* value;
    uint64_t timestamp;
} t_registro;

void memtable_create(void);

//Funcion para meterle nuevos elementos a la memtable
void memtable_new_elem(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp);

//Funcion para saber si existe area temporal de la tabla
bool memtable_has_table(char const* nombreTabla);

//Funcion para buscar segun una key dada el registro con mayor timestamp
t_registro* memtable_get_biggest_timestamp(char const* nombreTabla, uint16_t key);

//Funcion para eliminar un elemento de la memtable
void memtable_delete_table(char const* nombreTabla);

void memtable_dump(void);

#endif //LISSANDRA_MEMTABLE_H
