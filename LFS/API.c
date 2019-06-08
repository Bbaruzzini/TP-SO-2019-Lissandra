

#include "API.h"

int create(char* nombreTabla, char* tipoConsistencia, int numeroParticiones, int compactionTime)
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
    string_append(&dirTablas, "Tables");
//SI NO ANDA, CAMBIAR ESTE STRCOM POR UNA VERIFICACION SI LA TABLA=NULL
    if (strcmp(tabla, "") == 0)
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
        string_append(&dirTablas, tabla);
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
        tableMetadata = get_table_metadata(realpath, tabla);

        //Pruebas Brenda/Denise desde ACA
        printf("Por aca tambien paso\n");
        printf("Tabla: %s\n", tableMetadata->table);
        printf("Consistencia: %s\n", tableMetadata->consistency);
        printf("Particiones: %d\n", tableMetadata->partitions);
        printf("Tiempo: %d\n", tableMetadata->compaction_time);
        //HASTA ACA

        //free(dirTablas);
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
{/*
    //           cmd args
    //           0        1
    // sintaxis: DESCRIBE [name]

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DESCRIBE: Uso - DESCRIBE [tabla]");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
        table = tokens[1];

    DBRequest dbr;
    dbr.TableName = table;

    //ACA VA LA FUNCION DE DESCRIBE
  Metadata_Clear();  */ //ESTO LO PONEMOS POR LAS DUDAS PARA IR VIENDO SI ES NECESARIO O NO.
}

void HandleDrop(Vector const* args)
{
    (void) args;
}

