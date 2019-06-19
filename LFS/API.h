
#ifndef LFS_API_h__
#define LFS_API_h__

#include <Console.h>
#include <string.h>
#include <libcommons/string.h>
#include "LissandraLibrary.h"
#include "FileSystem.h"

void select_api(char const* nombreTabla, uint16_t key);
int insert(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp);
int create(char const* nombreTabla, uint8_t tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime);
void* describe(char const* nombreTabla);
int drop(char const* nombreTabla);

#endif //LFS_API_h__
