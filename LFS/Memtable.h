//
// Created by Denise on 07/06/2019.
//

#ifndef LISSANDRA_MEMTABLE_H
#define LISSANDRA_MEMTABLE_H

#include <stdint.h>
#include <vector.h>

typedef struct
{
    char* nombreTabla;
    Vector registros;
} t_elem_memtable;

//Querido Ariel: si de casualidad pasas por aca y lees esto, te agradeceria de corazon si me podes decir
//si el key, value y timestamp son int, int32_t, int16_t o cual de todos los int, porque la verdad verdad,
//no tengo la menor idea :D Graciaaaaaaas!
/// Ariel: esto es correcto
/// Obs2: value podria ser un valor fijo y no un char* asi no tienen que preocuparse por otro malloc mas
/// (miren como está hecho en memoria)
typedef struct
{
    uint16_t key;
    char* value;
    uint64_t timestamp;
} t_registro;

void crearMemtable(void);

//Funcion para crear un nuevo elemento del tipo t_elem_memtable
t_elem_memtable* new_elem_memtable(char const* nombreTabla);

//Funcion para crear nuevo elemento del tipo t_registro
t_registro* new_elem_registro(uint16_t key, char const* value, uint64_t timestamp);

//Funcion para meterle nuevos elementos a la memtable
void insert_new_in_memtable(t_elem_memtable* elemento);

//Funcion para meterle nuevos elementos (registros) al vector registros de un elemento de la memtable
//Se necesita el nombre de la tabla al cual agregar el registro
void insert_new_in_registros(char const* nombreTabla, t_registro* registro);

//Funcion para buscar, si existe, un elemento en la memTable para una cierta Tabla
//Si existe, lo retorna, sino retorna NULL
t_elem_memtable* memtable_get(char const* nombreTabla);

//Funcion para buscar segun una key dada el registro con mayor timestamp
t_registro* registro_get_biggest_timestamp(t_elem_memtable* elemento, uint16_t key);

//Funcion para eliminar un elemento de la memtable
int delete_elem_memtable(char const* nombreTabla);

void dump(void);
bool escribirBloque(char* pathBloque, size_t tamanioRegistro, char* buffer, int offset);
void generarPathBloque(int numBloque, char* buf);
size_t reservarNuevoBloqueLibre(char* pathArchivo);
void cambiarSize(char* pathArchivo, size_t newSize);

#endif //LISSANDRA_MEMTABLE_H
