
#ifndef Memoria_API_h
#define Memoria_API_h

#include "Frame.h"
#include <stdbool.h>
#include <stdint.h>
#include <vector.h>

// devuelve value o NULL si no se encuentra
// value debe apuntar a un espacio con (maxValueLength+1) bytes
void API_Select(char const* tableName, uint16_t key, char* value);

// en memoria va sin timestamp el request, se calcula luego. ver #1355
void API_Insert(char const* tableName, uint16_t key, char const* value);

// devuelve false si la tabla ya existe en el FS
bool API_Create(char const* tableName, char const* consistency, uint16_t partitions, uint32_t compactionTime);

// solicita al FS directamente
// results se encuentra construido con sizeof(struct MD)
void API_Describe(char const* tableName, Vector* results);

void API_Drop(char const* tableName);

void API_Journal(void);

#endif //Memoria_API_h
