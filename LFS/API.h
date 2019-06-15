
#ifndef LFS_API_h__
#define LFS_API_h__

#include <Console.h>
#include <string.h>
#include <libcommons/string.h>
#include "LissandraLibrary.h"
#include "FileSystem.h"

void select_api(char* nombreTabla, int key);
int insert(char* nombreTabla, uint16_t key, char* value, time_t timestamp); //MODIFICAR EL TIPO DE DATO DE TIMESTAMP
int create(char* nombreTabla, char* tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime);
void* describe(char* nombreTabla);
int drop(char* nombreTabla);

#endif //LFS_API_h__
