
#ifndef LFS_API_h__
#define LFS_API_h__

#include "FileSystem.h"
#include "LissandraLibrary.h"
#include <Console.h>
#include <libcommons/string.h>
#include <string.h>

void api_select(char* nombreTabla, uint16_t key);
uint8_t api_insert(char* nombreTabla, uint16_t key, char const* value, uint64_t timestamp);
uint8_t api_create(char* nombreTabla, uint8_t tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime);
void* api_describe(char* nombreTabla);
uint8_t api_drop(char* nombreTabla);

#endif //LFS_API_h__
