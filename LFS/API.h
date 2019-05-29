
#ifndef LFS_CLIHandlers_h__
#define LFS_CLIHandlers_h__

#include <Console.h>
#include <string.h>
#include <libcommons/string.h>
#include "LissandraLibrary.h"
#include "FileSystem.h"


//void select(char* nombreTabla,int key);
//void insert(char* nombreTabla, int key, char* value, int timeStamp); //MODIFICAR EL TIPO DE DATO DE TIMESTAMP
void create(char* nombreTabla, char* tipoConsistencia, int numeroParticiones, int compactionTime);
void* describe(char* tabla);
//void drop(char* nombreTabla);


CLICommandHandlerFn HandleSelect;
CLICommandHandlerFn HandleInsert;
CLICommandHandlerFn HandleCreate;
CLICommandHandlerFn HandleDescribe;
CLICommandHandlerFn HandleDrop;


#endif //LFS_CLIHandlers_h__
