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

/*
 * dictionary.c
 *
 *  Created on: 17/02/2012
 *      Author: fviale
 */

#include "dictionary.h"
#include "Malloc.h"

static unsigned int dictionary_hash(int key);
static void dictionary_resize(t_dictionary* d, unsigned int new_max_size);
static t_hash_element* dictionary_create_element(int key, unsigned int key_hash, void* val);
static t_hash_element* dictionary_get_element(t_dictionary const* me, int key);
static void* dictionary_remove_element(t_dictionary* me, int key);
static void dictionary_destroy_element(t_hash_element* element, void(*data_destroyer)(void* val));
static void internal_dictionary_clean_and_destroy_elements(t_dictionary* me, void(*data_destroyer)(void* val));

t_dictionary* dictionary_create(void)
{
    t_dictionary* me = Malloc(sizeof(t_dictionary));
    me->table_max_size = DEFAULT_DICTIONARY_INITIAL_SIZE;
    Vector_Construct(&me->elements, sizeof(t_hash_element*), NULL, 0);
    Vector_resize_zero(&me->elements, me->table_max_size);
    me->table_current_size = 0;
    me->elements_amount = 0;
    return me;
}

static unsigned int dictionary_hash(int key)
{
    unsigned int hash = 0x811C9DC5U;
    unsigned char* bytearray = (unsigned char*) &key;
    for (size_t idx = 0; idx < sizeof(int); ++idx)
    {
        hash ^= (unsigned int) bytearray[idx];
        hash *= 0x01000193U;
    }
    return hash;
}

void dictionary_put(t_dictionary* me, int key, void* val)
{
    t_hash_element* existing_element = dictionary_get_element(me, key);
    if (existing_element)
    {
        existing_element->value = val;
        return;
    }

    unsigned int key_hash = dictionary_hash(key);
    unsigned int index = key_hash % me->table_max_size;
    t_hash_element* new_element = dictionary_create_element(key, key_hash, val);

    t_hash_element* element = *((t_hash_element**) Vector_at(&me->elements, index));
    if (!element)
    {
        *((t_hash_element**) Vector_at(&me->elements, index)) = new_element;
        ++me->table_current_size;

        if (me->table_current_size >= me->table_max_size)
            dictionary_resize(me, me->table_max_size * 2);
    }
    else
    {
        while (element->next != NULL)
            element = element->next;

        element->next = new_element;
    }

    ++me->elements_amount;
}

void* dictionary_get(t_dictionary const* me, int key)
{
    t_hash_element* element = dictionary_get_element(me, key);
    return element != NULL ? element->value : NULL;
}

void* dictionary_remove(t_dictionary* me, int key)
{
    void* data = dictionary_remove_element(me, key);
    if (data != NULL)
        --me->elements_amount;

    return data;
}

void dictionary_remove_and_destroy(t_dictionary* me, int key, void(*data_destroyer)(void* val))
{
    void* value = dictionary_remove(me, key);
    if (value != NULL)
        data_destroyer(value);
}

void dictionary_iterate(t_dictionary const* d, void(*closure)(int key, void* val))
{
    for (unsigned int table_index = 0; table_index < d->table_max_size; ++table_index)
    {
        t_hash_element* element = *((t_hash_element**) Vector_at(&d->elements, table_index));
        while (element != NULL)
        {
            closure(element->key, element->value);
            element = element->next;
        }
    }
}

void dictionary_iterate_with_data(t_dictionary const* d, void(*closure)(int key, void* val, void* data), void* data)
{
    for (unsigned int table_index = 0; table_index < d->table_max_size; ++table_index)
    {
        t_hash_element* element = *((t_hash_element**) Vector_at(&d->elements, table_index));
        while (element != NULL)
        {
            closure(element->key, element->value, data);
            element = element->next;
        }
    }
}

void dictionary_clean(t_dictionary* me)
{
    internal_dictionary_clean_and_destroy_elements(me, NULL);
}

void dictionary_clean_and_destroy_elements(t_dictionary* me, void(*data_destroyer)(void* val))
{
    internal_dictionary_clean_and_destroy_elements(me, data_destroyer);
}

bool dictionary_has_key(t_dictionary const* me, int key)
{
    return dictionary_get_element(me, key) != NULL;
}

bool dictionary_is_empty(t_dictionary const* me)
{
    return !me->elements_amount;
}

unsigned int dictionary_size(t_dictionary const* me)
{
    return me->elements_amount;
}

void dictionary_destroy(t_dictionary* me)
{
    dictionary_clean(me);
    Vector_Destruct(&me->elements);
    Free(me);
}

void dictionary_destroy_and_destroy_elements(t_dictionary* me, void(*data_destroyer)(void*))
{
    dictionary_clean_and_destroy_elements(me, data_destroyer);
    Vector_Destruct(&me->elements);
    Free(me);
}

static void dictionary_resize(t_dictionary* me, unsigned int new_max_size)
{
    Vector* new_table = Malloc(sizeof(Vector));
    Vector_Construct(new_table, sizeof(t_hash_element*), NULL, 0);
    Vector_resize_zero(new_table, new_max_size);

    Vector* old_table = &me->elements;

    me->table_current_size = 0;

    for (unsigned int table_index = 0; table_index < me->table_max_size; ++table_index)
    {
        t_hash_element* old_element = *((t_hash_element**) Vector_at(old_table, table_index));
        t_hash_element* next_element;
        while (old_element != NULL)
        {
            // new position
            unsigned int new_index = old_element->hashcode % new_max_size;
            t_hash_element** elemAccessor = ((t_hash_element**) Vector_at(new_table, new_index));
            if (*elemAccessor == NULL)
            {
                *elemAccessor = old_element;
                ++me->table_current_size;
            }
            else
            {
                t_hash_element* new_element = *elemAccessor;
                while (new_element->next != NULL)
                    new_element = new_element->next;

                new_element->next = old_element;
            }

            next_element = old_element->next;
            old_element->next = NULL;
            old_element = next_element;
        }
    }

    me->elements = *new_table;
    me->table_max_size = new_max_size;
    Vector_Destruct(old_table);
}

/*
 * @NAME: dictionary_clean
 * @DESC: Destruye todos los elementos del diccionario
*/
static void internal_dictionary_clean_and_destroy_elements(t_dictionary* me, void(*data_destroyer)(void* val))
{
    for (unsigned int table_index = 0; table_index < me->table_max_size; ++table_index)
    {
        t_hash_element* element = *((t_hash_element**) Vector_at(&me->elements, table_index));
        while (element != NULL)
        {
            t_hash_element* next_element = element->next;
            dictionary_destroy_element(element, data_destroyer);
            element = next_element;
        }

        *((t_hash_element**) Vector_at(&me->elements, table_index)) = NULL;
    }

    me->table_current_size = 0;
    me->elements_amount = 0;
}

static t_hash_element* dictionary_create_element(int key, unsigned int key_hash, void* val)
{
    t_hash_element* element = Malloc(sizeof(t_hash_element));

    element->key = key;
    element->value = val;
    element->hashcode = key_hash;
    element->next = NULL;

    return element;
}

static t_hash_element* dictionary_get_element(t_dictionary const* me, int key)
{
    unsigned int index = dictionary_hash(key) % me->table_max_size;
    t_hash_element* element = *((t_hash_element**) Vector_at(&me->elements, index));
    if (!element)
        return NULL;

    do
    {
        if (element->key == key)
            return element;
    } while ((element = element->next) != NULL);

    return NULL;
}

static void* dictionary_remove_element(t_dictionary* me, int key)
{
    unsigned int index = dictionary_hash(key) % me->table_max_size;
    t_hash_element* element = *((t_hash_element**) Vector_at(&me->elements, index));
    if (!element)
        return NULL;

    if (element->key == key)
    {
        void* val = element->value;
        *((t_hash_element**) Vector_at(&me->elements, index)) = element->next;
        if (!*((t_hash_element**) Vector_at(&me->elements, index)))
            --me->table_current_size;

        Free(element);
        return val;
    }

    while (element->next != NULL)
    {
        if (element->next->key == key)
        {
            void* val = element->next->value;
            t_hash_element* aux = element->next;
            element->next = element->next->next;

            Free(aux);
            return val;
        }

        element = element->next;
    }

    return NULL;
}

static void dictionary_destroy_element(t_hash_element* element, void(*data_destroyer)(void* val))
{
    if (data_destroyer)
        data_destroyer(element->value);

    Free(element);
}
