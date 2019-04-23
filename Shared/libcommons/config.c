/*
 * Copyright (C) 2012 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
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

#include "dictionary.h"
#include "config.h"
#include "Malloc.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void add_configuration(char* line, void* cfg)
{
    t_config* const config = cfg;
    if (!string_starts_with(line, "#"))
    {
        // remover newlines de cualquier estilo
        line[strcspn(line, "\r\n")] = '\0';

        char** keyAndValue = string_n_split(line, 2, "=");
        if (!*keyAndValue)
        {
            // linea vacia
            Free(keyAndValue);
            return;
        }

        dictionary_put(config->properties, keyAndValue[0], keyAndValue[1]);
        Free(keyAndValue[0]);
        Free(keyAndValue);
    }
}

t_config* config_create(char const* path)
{
    FILE* file = fopen(path, "r");

    if (file == NULL)
        return NULL;

    struct stat stat_file;
    stat(path, &stat_file);

    t_config* config = Malloc(sizeof(t_config));

    config->path = strdup(path);
    config->properties = dictionary_create();

    char* buffer = Calloc(stat_file.st_size + 1, 1);
    fread(buffer, 1, stat_file.st_size, file);

    char** lines = string_split(buffer, "\n");

    string_iterate_lines_with_data(lines, add_configuration, config);
    string_iterate_lines(lines, (void(*)(char*)) Free);

    Free(lines);
    Free(buffer);
    fclose(file);

    return config;
}

bool config_has_property(t_config const* self, char const* key)
{
    return dictionary_has_key(self->properties, key);
}

char* config_get_string_value(t_config const* self, char const* key)
{
    return dictionary_get(self->properties, key);
}

int config_get_int_value(t_config const* self, char const* key)
{
    char* value = config_get_string_value(self, key);
    return atoi(value);
}

long config_get_long_value(t_config const* self, char const* key)
{
    char* value = config_get_string_value(self, key);
    return atol(value);
}

double config_get_double_value(t_config const* self, char const* key)
{
    char* value = config_get_string_value(self, key);
    return atof(value);
}

char** config_get_array_value(t_config const* self, char const* key)
{
    char* value_in_dictionary = config_get_string_value(self, key);
    return string_get_string_as_array(value_in_dictionary);
}

int config_keys_amount(t_config const* self)
{
    return dictionary_size(self->properties);
}

void config_destroy(t_config* config)
{
    if (!config)
        return;

    dictionary_destroy_and_destroy_elements(config->properties, Free);
    Free(config->path);
    Free(config);
}

void config_set_value(t_config* self, char const* key, char const* value)
{
    config_remove_key(self, key);
    char* duplicate_value = string_duplicate(value);
    dictionary_put(self->properties, key, duplicate_value);
}

void config_remove_key(t_config* self, char const* key)
{
    t_dictionary* dictionary = self->properties;
    if (dictionary_has_key(dictionary, key))
        dictionary_remove_and_destroy(dictionary, key, Free);
}

int config_save(t_config const* self)
{
    return config_save_in_file(self, self->path);
}

static void add_line(char const* key, void* value, void* ln)
{
    char** lines = ln;
    string_append_with_format(lines, "%s=%s\n", key, value);
}

int config_save_in_file(t_config const* self, char const* path)
{
    FILE* file = fopen(path, "wb+");
    if (file == NULL)
        return -1;

    char* lines = string_new();

    dictionary_iterator_with_data(self->properties, add_line, &lines);
    int result = fwrite(lines, 1, strlen(lines), file);
    fclose(file);
    Free(lines);
    return result;
}
