
#ifndef LFS_API_h__
#define LFS_API_h__

#include "FileSystem.h"
#include "LissandraLibrary.h"
#include <Console.h>
#include <libcommons/string.h>
#include <string.h>

void select_api(char const* nombreTabla, uint16_t key);
uint8_t insert(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp);
uint8_t create(char const* nombreTabla, uint8_t tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime);
void* describe(char const* nombreTabla);
uint8_t drop(char const* nombreTabla);

#endif //LFS_API_h__
