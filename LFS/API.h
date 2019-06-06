
#ifndef LFS_API_h__
#define LFS_API_h__

#include <Console.h>
#include <string.h>
#include <libcommons/string.h>
#include "LissandraLibrary.h"
#include "FileSystem.h"


//void select(char* nombreTabla,int key);
//void insert(char* nombreTabla, int key, char* value, int timeStamp); //MODIFICAR EL TIPO DE DATO DE TIMESTAMP
int create(char* nombreTabla, char* tipoConsistencia, int numeroParticiones, int compactionTime);
void* describe(char* tabla);
//void drop(char* nombreTabla);


CLICommandHandlerFn HandleSelect;
CLICommandHandlerFn HandleInsert;
CLICommandHandlerFn HandleCreate;
CLICommandHandlerFn HandleDescribe;
CLICommandHandlerFn HandleDrop;


#endif //LFS_API_h__
