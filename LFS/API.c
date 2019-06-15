

#include "API.h"

void select_api(char* nombreTabla, int key)
{

}

int insert(char* nombreTabla, uint16_t key, char* value, time_t timestamp)
{
    //Verifica si la tabla existe en el File System
    char* path = generarPathTabla(nombreTabla);

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
    if (!hayDump(nombreTabla))
    {

        t_elem_memtable* newElem = new_elem_memtable(nombreTabla);
        insert_new_in_memtable(newElem);

    }
    //Si el parametro timestamp es nulo, se obtiene el valor actual del Epoch UNIX
    if (timestamp == 0)
    {
        timestamp = (unsigned) time(NULL);
    }

    t_registro* newReg = new_elem_registro(key, value, timestamp);
    insert_new_in_registros(nombreTabla, newReg);

    LISSANDRA_LOG_INFO("Se inserto un nuevo registro en la tabla %s", nombreTabla);

    return EXIT_SUCCESS;
}

int create(char* nombreTabla, char* tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime)
{
    //Como los nombres de las tablas deben estar en uppercase, primero me aseguro de que así sea y luego genero el path de esa tabla
    char nomTabla[100];
    strcpy(nomTabla, nombreTabla);
    string_to_upper(nomTabla);
    nombreTabla = nomTabla; //Se que esto probablemente no es necesario, pero como no estaba segura donde mas
    //aparece "nombreTabla, lo puse asi xD"

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

        free(pathMetadataTabla);

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
                    printf("ERROR: No hay espacio en el File System\n");
                    free(pathParticion);
                    free(path);
                    //TODO: Aca haría un drop(nombreTabla), porque si no puede crear todas las particiones
                    // para una tabla nueva, no tiene sentido que la tabla exista en si.
                    // La otra opción es modificar el codigo de create() para que primero analice si hay
                    // suficientes bloques libres para crear la tabla, pero como el proceso FS en si va a atender
                    // peticiones de create en paralelo, lo que puede pasar es que dos peticiones pregunten si hay espacio libre
                    // reciban un si como rta y luego alguna o ninguna obtenga todos los bloques que necesita porque se los uso
                    // la otra peticion...
                    int resDrop = drop(nombreTabla);
                    if (resDrop == EXIT_FAILURE)
                    {
                        LISSANDRA_LOG_ERROR("Se produjo un error intentado borrar la tabla %s", nombreTabla);
                    }
                    return EXIT_FAILURE;

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
        free(path);
        return EXIT_SUCCESS;

    }
    else
    {

        LISSANDRA_LOG_ERROR("La tabla ya existe en el File System");
        printf("ERROR: La tabla ya existe en el File System\n");
        free(path);
        return EXIT_FAILURE;

    }

}


//Verificar que la tabla exista en el file system.
//Eliminar directorio y todos los archivos de dicha tabla.

int drop(char* nombreTabla)
{

    LISSANDRA_LOG_INFO("Se esta borrando la tabla...%s", nombreTabla);

    char* pathAbsoluto = generarPathTabla(nombreTabla);

    if(!existeDir(pathAbsoluto)){
        LISSANDRA_LOG_ERROR("La tabla no existe...");
        printf("La tabla no existe\n");
        return EXIT_FAILURE;
    }

    t_elem_memtable* elemento = memtable_get(nombreTabla);

    if (elemento != NULL)
    {
        //Se elimina la memtable de la tabla
        int resMemtable = delete_elem_memtable(nombreTabla);

        if (resMemtable != 0)
        {
            LISSANDRA_LOG_ERROR("Se produjo un error al intentar borrar de la memtable la tabla: %s", nombreTabla);
            return EXIT_FAILURE;
        }
    }

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


void* describe(char* nombreTabla)
{

    char* dirTablas;
    char* realpath;

    dirTablas = string_new();
    string_append(&dirTablas, confLFS->PUNTO_MONTAJE);
    string_append(&dirTablas, "Tables");
//SI NO ANDA, CAMBIAR ESTE STRCOM POR UNA VERIFICACION SI LA TABLA=NULL
    if (nombreTabla == NULL)
    {
        realpath = dirTablas;
        t_list* listTableMetadata;
        listTableMetadata = list_create();

        int resultado = traverse(realpath, listTableMetadata,
                                 ""); //No se si aca se le puede pasar "", pero quiero pasar vacio

        if (resultado == 0)
        {
            //free(dirTablas);
            free(realpath);
            return listTableMetadata;
        }
        else
        {
            printf("ERROR: Se produjo un error al recorrer el directorio /Tables");
            //free(dirTablas);
            free(realpath);
            return NULL;
        }

    }
    else
    {
        string_append(&dirTablas, "/");
        string_append(&dirTablas, nombreTabla);
        realpath = dirTablas;

        if (!existeDir(realpath))
        {
            printf("ERROR: La ruta especificada es invalida\n");
            //free(dirTablas);
            free(realpath);
            return NULL;
        }

        string_append(&dirTablas, "/Metadata.bin");
        realpath = dirTablas;

        t_describe* tableMetadata = Malloc(sizeof(t_describe));
        tableMetadata = get_table_metadata(realpath, nombreTabla);

        //Pruebas Brenda/Denise desde ACA
        /*
        printf("Por aca tambien paso\n");
        printf("Tabla: %s\n", tableMetadata->table);
        printf("Consistencia: %s\n", tableMetadata->consistency);
        printf("Particiones: %d\n", tableMetadata->partitions);
        printf("Tiempo: %d\n", tableMetadata->compaction_time);
         */
        //HASTA ACA

        //free(dirTablas);
        free(realpath);
        return tableMetadata;

    }

}




