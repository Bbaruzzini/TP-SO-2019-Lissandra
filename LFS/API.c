

#include "API.h"

void create(char* nombreTabla, char* tipoConsistencia, int numeroParticiones, int compactionTime)
{

    //Como los nombres de las tablas deben estar en uppercase, primero me aseguro de que así sea y luego genero el path de esa tabla
    string_capitalized(nombreTabla);
    char* path = generarPathTabla(nombreTabla);

    //Evalua si existe la tabla
    bool existe = existeDir(path);

    //Si la tabla existe hace un log y muestra error
    //Si la tabla no existe la crea, crea su metadata y las particiones
    if (!existe)
    {

        mkdir(path, 0700);

        //Crea el path de la metadata de la tabla y le carga los datos
        char* pathMetadataTabla = string_new();
        string_append(&pathMetadataTabla, path);
        string_append(&pathMetadataTabla, "/Metadata.bin");

        FILE* metadata = fopen(pathMetadataTabla, "w");
        fprintf(metadata, "CONSISTENCY=%s\n", tipoConsistencia);
        fprintf(metadata, "PARTITIONS=%d\n", numeroParticiones);
        fprintf(metadata, "COMPACTION_TIME=%d\n", compactionTime);
        fclose(metadata);

        //Crea cada particion, le carga los datos y le asigna un bloque
        int j;
        FILE* particion;

        for (j = 0; j < numeroParticiones; j++)
        {

            char* pathParticion = string_new();
            string_append(&pathParticion, path);
            string_append(&pathParticion, "/");
            string_append(&pathParticion, string_itoa(j));
            string_append(&pathParticion, ".bin");

            if (!existeArchivo(pathParticion))
            {

                int bloqueLibre = buscarBloqueLibre();

                if (bloqueLibre == -1)
                {

                    LISSANDRA_LOG_ERROR("No hay espacio en el File System");
                    printf("ERROR: No hay espacio en el File System");
                    return;

                }

                escribirValorBitarray(1, bloqueLibre);

                particion = fopen(pathParticion, "a");
                fprintf(particion, "SIZE=0\n");
                fprintf(particion, "BLOCKS=[%d]\n", bloqueLibre);
                fclose(particion);

            }

            free(pathParticion);

        }

        LISSANDRA_LOG_DEBUG("Se finalizo la creacion de la tabla");

    }
    else
    {

        LISSANDRA_LOG_ERROR("La tabla ya existe en el File System");
        printf("ERROR: La tabla ya existe en el File System");

    }

    free(path);

}


void HandleSelect(Vector const* args)
{
    (void) args;
}

void HandleInsert(Vector const* args)
{
    (void) args;
}

void HandleCreate(Vector const* args)
{
    (void) args;
}

void HandleDescribe(Vector const* args)
{
    (void) args;
}

void HandleDrop(Vector const* args)
{
    (void) args;
}

