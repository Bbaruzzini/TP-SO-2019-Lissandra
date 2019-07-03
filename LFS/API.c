
#include "API.h"
#include "Config.h"
#include "Memtable.h"
#include <Consistency.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void api_select(char* nombreTabla, uint16_t key)
{
    char path[PATH_MAX];
    generarPathTabla(nombreTabla, path);

    /// todo
}

uint8_t api_insert(char* nombreTabla, uint16_t key, char const* value, uint64_t timestamp)
{
    //Verifica si la tabla existe en el File System
    char path[PATH_MAX];
    generarPathTabla(nombreTabla, path);

    if (!existeDir(path))
    {

        LISSANDRA_LOG_ERROR("La tabla ingresada para INSERT: %s no existe en el File System", nombreTabla);
        printf("ERROR: La tabla ingresada: %s no existe en el File System\n", nombreTabla);
        return EXIT_FAILURE;

    }

    //Esto comentado creo que no va. No recuerdo ni por que lo hice...
    /*
    //Si la tabla existe, carga los datos de su metadata en tableMetadata
    char* pathMetadataTabla = string_new();
    string_append(&pathMetadataTabla, path);
    string_append(&pathMetadataTabla, "/Metadata.bin");

    t_describe* tableMetadata;
    tableMetadata = get_table_metadata(pathMetadataTabla, nombreTabla);
    */

    //Verifica si hay datos a dumpear, y si no existen aloca memoria
    new_elem_memtable(nombreTabla, key, value, timestamp);

    LISSANDRA_LOG_INFO("Se inserto un nuevo registro en la tabla %s", nombreTabla);
    return EXIT_SUCCESS;
}

uint8_t api_create(char* nombreTabla, uint8_t tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime)
{
    char path[PATH_MAX];
    generarPathTabla(nombreTabla, path);

    //Evalua si existe la tabla
    //Si la tabla existe hace un log y muestra error
    //Si la tabla no existe la crea, crea su metadata y las particiones
    if (!existeDir(path))
    {
        mkdir(path, 0700);

        //Crea el path de la metadata de la tabla y le carga los datos
        char pathMetadataTabla[PATH_MAX];
        snprintf(pathMetadataTabla, PATH_MAX, "%s/Metadata.bin", path);

        {
            FILE* metadata = fopen(pathMetadataTabla, "w");
            fprintf(metadata, "CONSISTENCY=%s\n", CriteriaString[tipoConsistencia].String);
            fprintf(metadata, "PARTITIONS=%d\n", numeroParticiones);
            fprintf(metadata, "COMPACTION_TIME=%d\n", compactionTime);
            fclose(metadata);
        }

        //Crea cada particion, le carga los datos y le asigna un bloque
        for (uint16_t j = 0; j < numeroParticiones; ++j)
        {
            char pathParticion[PATH_MAX];
            snprintf(pathParticion, PATH_MAX, "%s/%hu.bin", path, j);

            if (!existeArchivo(pathParticion))
            {
                size_t bloqueLibre;
                if (!buscarBloqueLibre(&bloqueLibre))
                {
                    LISSANDRA_LOG_ERROR("No hay espacio en el File System");
                    printf("ERROR: No hay espacio en el File System\n");

                    //TODO: Aca haría un api_drop(nombreTabla), porque si no puede crear todas las particiones
                    // para una tabla nueva, no tiene sentido que la tabla exista en si.
                    // La otra opción es modificar el codigo de create() para que primero analice si hay
                    // suficientes bloques libres para crear la tabla, pero como el proceso FS en si va a atender
                    // peticiones de create en paralelo, lo que puede pasar es que dos peticiones pregunten si hay espacio libre
                    // reciban un si como rta y luego alguna o ninguna obtenga todos los bloques que necesita porque se los uso
                    // la otra peticion...
                    uint8_t resDrop = api_drop(nombreTabla);
                    if (resDrop == EXIT_FAILURE)
                        LISSANDRA_LOG_ERROR("Se produjo un error intentado borrar la tabla %s", nombreTabla);

                    return EXIT_FAILURE;

                }

                {
                    FILE* particion = fopen(pathParticion, "a");
                    fprintf(particion, "SIZE=0\n");
                    fprintf(particion, "BLOCKS=[%d]\n", bloqueLibre);
                    fclose(particion);
                }
            }
        }

        LISSANDRA_LOG_DEBUG("Se finalizo la creacion de la tabla");
        return EXIT_SUCCESS;

    }
    else
    {
        LISSANDRA_LOG_ERROR("La tabla ya existe en el File System");
        printf("ERROR: La tabla ya existe en el File System\n");
        return EXIT_FAILURE;
    }
}


//Verificar que la tabla exista en el file system.
//Eliminar directorio y todos los archivos de dicha tabla.

uint8_t api_drop(char* nombreTabla)
{
    char pathAbsoluto[PATH_MAX];
    generarPathTabla(nombreTabla, pathAbsoluto);

    LISSANDRA_LOG_INFO("Se esta borrando la tabla...%s", nombreTabla);

    if (!existeDir(pathAbsoluto))
    {
        LISSANDRA_LOG_ERROR("La tabla no existe...");
        printf("La tabla no existe\n");
        return EXIT_FAILURE;
    }

    t_elem_memtable* elemento = memtable_get(nombreTabla);
    if (!elemento)
    {
        LISSANDRA_LOG_ERROR("Se produjo un error al intentar borrar de la memtable la tabla: %s", nombreTabla);
        return EXIT_FAILURE;
    }

    //Se elimina la memtable de la tabla
    delete_elem_memtable(nombreTabla);

    //Se eliminan los archivos de la tabla
    int resultado = traverse_to_drop(pathAbsoluto, nombreTabla);
    if (resultado != 0)
    {
        LISSANDRA_LOG_ERROR("Se produjo un error al intentar borrar la tabla: %s", nombreTabla);
        return EXIT_FAILURE;
    }

    remove(pathAbsoluto);
    //Se elimina la informacion administrativa de la tabla (?)

    LISSANDRA_LOG_INFO("Tabla %s borrada con exito", nombreTabla);
    return EXIT_SUCCESS;
}


void* api_describe(char* nombreTabla)
{
    //SI NO ANDA, CAMBIAR ESTE STRCOM POR UNA VERIFICACION SI LA TABLA=NULL
    if (nombreTabla == NULL)
    {
        char dirTablas[PATH_MAX];
        snprintf(dirTablas, PATH_MAX, "%sTables", confLFS.PUNTO_MONTAJE);

        t_list* listTableMetadata = list_create();
        int resultado = traverse(dirTablas, listTableMetadata, NULL);
        if (resultado == 0)
            return listTableMetadata;

        printf("ERROR: Se produjo un error al recorrer el directorio /Tables");
        return NULL;
    }

    char pathTableMeta[PATH_MAX];
    generarPathTabla(nombreTabla, pathTableMeta);
    strcat(pathTableMeta, "/Metadata.bin");

    if (!existeArchivo(pathTableMeta))
    {
        printf("ERROR: La ruta especificada es invalida\n");
        return NULL;
    }

    //Pruebas Brenda/Denise desde ACA
    /*t_describe* tableMetadata = get_table_metadata(pathMetadata, nombreTabla);

    printf("Por aca tambien paso\n");
    printf("Tabla: %s\n", tableMetadata->table);
    printf("Consistencia: %s\n", tableMetadata->consistency);
    printf("Particiones: %d\n", tableMetadata->partitions);
    printf("Tiempo: %d\n", tableMetadata->compaction_time);
     */
    //HASTA ACA

    return get_table_metadata(pathTableMeta, nombreTabla);
}
