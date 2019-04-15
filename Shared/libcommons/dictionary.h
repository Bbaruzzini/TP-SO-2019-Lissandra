/*
 * Copyright (C) 2012 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is Free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Modificaciones by ariel-: 20190910
 * Cambio de key por 'int', este diccionario se va a usar para mapear fds con su abstracción
 * Ahora utiliza el TAD "vector" en lugar de hacer los alloc a mano
 */

#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#define DEFAULT_DICTIONARY_INITIAL_SIZE 20

#include "vector.h"
#include <stdbool.h>

struct hash_element
{
    int key;
    unsigned int hashcode;
    void* value;
    struct hash_element* next;
};
typedef struct hash_element t_hash_element;

typedef struct
{
    Vector elements;
    unsigned int table_max_size;
    unsigned int table_current_size;
    unsigned int elements_amount;
} t_dictionary;

/**
* @NAME: dictionary_create
* @DESC: Crea el diccionario
*/
t_dictionary* dictionary_create(void);

/**
* @NAME: dictionary_put
* @DESC: Inserta un nuevo par (key->value) al diccionario, en caso de ya existir la key actualiza el value.
* [Warning] - Tener en cuenta que esto no va a liberar la memoria del `value` original.
*/
void dictionary_put(t_dictionary* d, int key, void* val);

/**
* @NAME: dictionary_get
* @DESC: Obtiene value asociado a key.
*/
void* dictionary_get(t_dictionary const* d, int key);

/**
* @NAME: dictionary_remove
* @DESC: Remueve un elemento del diccionario y lo retorna.
*/
void* dictionary_remove(t_dictionary* d, int key);

/**
* @NAME: dictionary_remove_and_destroy
* @DESC: Remueve un elemento del diccionario y lo destruye.
*/
void dictionary_remove_and_destroy(t_dictionary* d, int key, void(*data_destroyer)(void* val));

/**
* @NAME: dictionary_iterate
* @DESC: Aplica closure a todos los elementos del diccionario.
*
* La función que se pasa por paremtro recibe (int key, void* value)
*/
void dictionary_iterate(t_dictionary const* d, void(*closure)(int key, void* val));

/**
* @NAME: dictionary_iterate_with_data
* @DESC: Aplica closure a todos los elementos del diccionario.
* Se permite pasar una estructura definida por el usuario a la función.
*
* La función que se pasa por paremtro recibe (int key, void* value, void* data)
*/
void dictionary_iterate_with_data(t_dictionary const* d, void(*closure)(int key, void* val, void* data), void* data);

/**
* @NAME: dictionary_clean
* @DESC: Quita todos los elementos del diccionario
*/
void dictionary_clean(t_dictionary* d);

/**
* @NAME: dictionary_clean_and_destroy_elements
* @DESC: Quita todos los elementos del diccionario y los destruye
*/
void dictionary_clean_and_destroy_elements(t_dictionary* d, void(*data_destroyer)(void* val));

/**
* @NAME: dictionary_has_key
* @DESC: Retorna true si key se encuentra en el diccionario
*/
bool dictionary_has_key(t_dictionary const* d, int key);

/**
* @NAME: dictionary_is_empty
* @DESC: Retorna true si el diccionario está vacío
*/
bool dictionary_is_empty(t_dictionary const* d);

/**
* @NAME: dictionary_size
* @DESC: Retorna la cantidad de elementos del diccionario
*/
unsigned int dictionary_size(t_dictionary const* d);

/**
* @NAME: dictionary_destroy
* @DESC: Destruye el diccionario
*/
void dictionary_destroy(t_dictionary* d);

/**
* @NAME: dictionary_destroy_and_destroy_elements
* @DESC: Destruye el diccionario y destruye sus elementos
*/
void dictionary_destroy_and_destroy_elements(t_dictionary* d, void(*data_destroyer)(void* val));

#endif /* DICTIONARY_H_ */
