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

#include "string.h"
#include "Malloc.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static void _string_do(char* text, void (* closure)(char*));
static void _string_lower_element(char* ch);
static void _string_upper_element(char* ch);
static void _string_append_with_format_list(const char* format, char** original, va_list arguments);
static char** _string_split(char const* text, char* separator, bool(*condition)(char const*, int));

char* string_repeat(char character, int count)
{
    char* text = Calloc(count + 1, 1);
    int i = 0;
    for (i = 0; i < count; ++i)
    {
        text[i] = character;
    }
    return text;
}

char* string_duplicate(char const* original)
{
    return strdup(original);
}

void string_append(char** original, char const* string_to_add)
{
    *original = Realloc(*original, strlen(*original) + strlen(string_to_add) + 1);
    strcat(*original, string_to_add);
}

char* string_new()
{
    return string_duplicate("");
}

char* string_from_format(const char* format, ...)
{
    char* nuevo;
    va_list arguments;
    va_start(arguments, format);
    nuevo = string_from_vformat(format, arguments);
    va_end(arguments);
    return nuevo;
}

char* string_from_vformat(const char* format, va_list arguments)
{
    char* nuevo = string_new();
    _string_append_with_format_list(format, &nuevo, arguments);
    return nuevo;
}

char* string_itoa(int number)
{
    return string_from_format("%d", number);
}

void string_append_with_format(char** original, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    _string_append_with_format_list(format, original, arguments);
    va_end(arguments);
}

void string_to_upper(char* text)
{
    _string_do(text, &_string_upper_element);
}

void string_to_lower(char* text)
{
    _string_do(text, &_string_lower_element);
}

void string_capitalized(char* text)
{
    if (!string_is_empty(text))
    {
        _string_upper_element(text);
        if (strlen(text) >= 2)
        {
            string_to_lower(&text[1]);
        }
    }
}

void string_trim(char** text)
{
    string_trim_left(text);
    string_trim_right(text);
}

void string_trim_left(char** text)
{
    char* string_without_blank = *text;

    while (isblank(*string_without_blank))
    {
        string_without_blank++;
    }

    char* new_string = string_duplicate(string_without_blank);

    Free(*text);
    *text = new_string;
}

void string_trim_right(char** text)
{
    char* string_without_blank = *text;
    int i = strlen(*text) - 1;
    while (i >= 0 && isspace(string_without_blank[i]))
    {
        string_without_blank[i] = '\0';
        i--;
    }
    *text = Realloc(*text, strlen(string_without_blank) + 1);
}

bool string_is_empty(char* text)
{
    return strlen(text) == 0;
}

bool string_starts_with(char* text, char* begin)
{
    return strncmp(text, begin, strlen(begin)) == 0;
}

bool string_ends_with(char* text, char* end)
{
    if (strlen(text) < strlen(end))
    {
        return false;
    }

    int index = strlen(text) - strlen(end);
    return strcmp(&text[index], end) == 0;
}

bool string_equals_ignore_case(char* actual, char* expected)
{
    return strcasecmp(actual, expected) == 0;
}

char** string_split(char const* text, char* separator)
{
    bool _is_last_token(char const* next, int _)
    {
        (void) _;
        return next[0] != '\0';
    }
    return _string_split(text, separator, _is_last_token);
}

char** string_n_split(char const* text, int n, char* separator)
{
    bool _is_last_token(char const* next, int index)
    {
        return next[0] != '\0' && index < (n - 1);
    }
    return _string_split(text, separator, _is_last_token);
}

char** string_get_string_as_array(char* text)
{
    int length_value = strlen(text) - 2;
    char* value_without_brackets = string_substring(text, 1, length_value);
    char** array_values = string_split(value_without_brackets, ",");

    int i = 0;
    while (array_values[i] != NULL)
    {
        string_trim(&(array_values[i]));
        ++i;
    }

    Free(value_without_brackets);
    return array_values;
}

char* string_substring(char const* text, int start, int length)
{
    char* new_string = Calloc(1, length + 1);
    strncpy(new_string, text + start, length);
    return new_string;
}

char* string_substring_from(char const* text, int start)
{
    return string_substring(text, start, strlen(text) - start);
}

char* string_substring_until(char const* text, int length)
{
    return string_substring(text, 0, length);
}

void string_iterate_lines(char** strings, void (*closure)(char*))
{
    while (*strings != NULL)
    {
        closure(*strings);
        strings++;
    }
}

void string_iterate_lines_with_data(char** strings, void (*closure)(char*, void*), void* extra)
{
    while (*strings != NULL)
    {
        closure(*strings, extra);
        strings++;
    }
}

int string_length(char const* text)
{
    return strlen(text);
}

char* string_reverse(char const* palabra)
{
    char* resultado = Calloc(1, string_length(palabra) + 1);

    int i = string_length(palabra) - 1, j = 0;
    while (i >= 0)
    {
        resultado[j] = palabra[i];
        --i;
        ++j;
    }

    return resultado;
}

bool string_contains(char const* text, char const* substring)
{
    return strstr(text, substring) != NULL;
}

void string_replace(char* text, char viejo, char nuevo)
{
    for (char* i = text; *i != '\0'; ++i)
        if (*i == viejo)
            *i = nuevo;
}

/** PRIVATE FUNCTIONS **/

static void _string_upper_element(char* ch)
{
    *ch = toupper(*ch);
}

static void _string_lower_element(char* ch)
{
    *ch = tolower(*ch);
}

static void _string_do(char* text, void (* closure)(char* c))
{
    int i = 0;
    while (text[i] != '\0')
    {
        closure(&text[i]);
        i++;
    }
}


static void _string_append_with_format_list(const char* format, char** original, va_list arguments)
{
    va_list copy_arguments;
    va_copy(copy_arguments, arguments);
    size_t buffer_size = vsnprintf(NULL, 0, format, copy_arguments) + 1;
    va_end(copy_arguments);

    char* temporal = Malloc(buffer_size);
    va_copy(copy_arguments, arguments);
    vsnprintf(temporal, buffer_size, format, copy_arguments);
    va_end(copy_arguments);
    string_append(original, temporal);
    Free(temporal);
}

static char** _string_split(char const* text, char* separator, bool(*condition)(char const*, int))
{
    char** substrings = NULL;
    int size = 0;

    char* text_to_iterate = string_duplicate(text);

    char* next = text_to_iterate;
    char* str = text_to_iterate;

    while (condition(next, size))
    {
        char* token = strtok_r(str, separator, &next);
        if (token == NULL)
            break;

        str = NULL;
        ++size;
        substrings = Realloc(substrings, sizeof(char*) * size);
        substrings[size - 1] = string_duplicate(token);
    };

    if (next[0] != '\0')
    {
        ++size;
        substrings = Realloc(substrings, sizeof(char*) * size);
        substrings[size - 1] = string_duplicate(next);
    }

    ++size;
    substrings = Realloc(substrings, sizeof(char*) * size);
    substrings[size - 1] = NULL;

    Free(text_to_iterate);
    return substrings;
}
