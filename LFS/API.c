

#include "API.h"

void create(char* nombreTabla, char* tipoConsistencia, int numeroParticiones, int compactionTime)
{

    //Como los nombres de las tablas deben estar en uppercase, primero me aseguro de que así sea y luego genero el path de esa tabla
    string_to_upper(nombreTabla);
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

/*
//Verificar que la tabla exista en el file system.
//Eliminar directorio y todos los archivos de dicha tabla.

void drop(char* nombreTabla){
    LOG_LEVEL_INFO("Se esta borrando la tabla...%s\n ", nombreTabla);

    char* pathAbsoluto = generarPathTabla(nombreTabla);

    if(!existeDir(pathAbsoluto)){
        LOG_LEVEL_ERROR("La tabla no existe...");
        printf ("La tabla no existe");

    }
   // En conclusión, debes:
   // 1. Eliminar la memtable de esa tabla
   // 2. Eliminar los archivos de esa tabla (.bin/.tmp/metadata) (También borrado de los bloques)
   // 3. Eliminar el directorio de la tabla
   //  4. Eliminar toda la información administrativa de la tabla
    else{
        t_config * data = config_create(pathAbsoluto);
        char** bloques = config_get_array_value(data, "BLOQUES");

        int j = 0;

        while(bloques[j] != NULL){
            log_info(logger, "Bloque a liberar: %d", atoi(bloques[j]));
            escribirValorBitarray(0, atoi(bloques[j]));
            j++;
        }

        unlink(pathAbsoluto);
        log_debug(logger, "ARCHIVO BORRADO CON EXITO");

        enviar(socketKernel, BORRAR_ARCHIVO_OK, sizeof(int), &j);

        config_destroy(data);
        free(pathAbsoluto);

    }
    return;
}
*/

void* describe(char* tabla)
{

    char* dirTablas;
    char* realpath;

    dirTablas = string_new();
    string_append(&dirTablas, confLFS->PUNTO_MONTAJE);
    string_append(&dirTablas, "Tablas");

    if (strcmp(tabla, "") == 0)
    {

        realpath = dirTablas;
        t_list* listTableMetadata;
        listTableMetadata = list_create();

        int resultado = traverse(realpath, listTableMetadata,
                                 ""); //No se si aca se le puede pasar "", pero quiero pasar vacio

        if (resultado == 0)
        {

            free(dirTablas);
            free(realpath);
            return listTableMetadata;

        }
        else
        {

            printf("ERROR: Se produjo un error al recorrer el directorio /Tablas");
            free(dirTablas);
            free(realpath);
            return -1;

        }

    }
    else
    {

        string_append(&dirTablas, "/");
        string_append(&dirTablas, tabla);
        realpath = dirTablas;

        if (!existeDir(realpath))
        {

            printf("ERROR: La ruta especificada es invalida\n");
            free(dirTablas);
            free(realpath);
            return -1;

        }

        string_append(&dirTablas, "/Metadata.bin");
        realpath = dirTablas;

        t_describe* tableMetadata = get_table_metadata(realpath, tabla);

        free(dirTablas);
        free(realpath);
        return tableMetadata;

    }

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

