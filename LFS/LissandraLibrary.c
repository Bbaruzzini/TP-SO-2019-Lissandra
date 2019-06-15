//
// Created by Denise on 26/04/2019.
//

//Linkear bibliotecas!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#include "LissandraLibrary.h"


//Variables
//static hace que las variables no se puedan referenciar desde otro .c utilizando 'extern'
//si lo desean cambiar, quitenlo
static Socket* sock_LFS = NULL;

static LockedQueue* lista_pedidos = NULL;

static sem_t sem_pedido = { 0 }; //Cuando hagamos copy paste, cambiar el semaforo "pedido" por "sem_pedido"

static pthread_t hilo_atender_memoria;

t_pedido* obtener_pedido(void)
{
    return LockedQueue_Next(lista_pedidos);
}

void* atender_pedidos(void* arg)
{
    (void) arg;

    while (ProcessRunning)
    {
        //----Utilizo un semáforo (originalmente iniciado en 0) para saber cuándo hay pedidos en la lista de pedidos y atenderlos.
        sem_wait(&sem_pedido);

        //----Busco el pedido que hizo esta memoria
        //----La lista ya trae un mutex por lo que sencillamente extraigo el pedido
        t_pedido* pedido = obtener_pedido();

        // TODO: implementar logica, dejo comentado para que compile
/*
        //----Segun el id del pedido, ejecuto el procedimiento correspondiente
        switch (pedido->id)
        {
            case SELECT:
                funcion_select(pedido);
                break;
            case INSERT:
                funcion_insert(pedido);
                break;
            case CREATE:
                funcion_create(pedido);
                break;
            case DESCRIBE:
                funcion_describe(pedido);
                break;
            case DROP:
                funcion_drop(pedido);
                break;
        }

        free(pedido->path);
*/
        free(pedido);
    }

    return NULL;
}

void* atender_memoria(void* socketMemoria)
{
    printf("Y tambien paso por atender_memoria\n");

    while (ProcessRunning)
    {
        if (!Socket_HandlePacket(socketMemoria))
        {
            /// ya hace logs si hubo errores, cerrar conexión
            Socket_Destroy(socketMemoria);
            return (void*) false;
        }
    }

    return (void*) true;
}

void memoria_conectar(Socket* fs, Socket* memoriaNueva)
{
    (void) fs;

    printf("Paso por memoria_conectar\n");
    //Recibe el handshake
    Packet* p = Socket_RecvPacket(memoriaNueva);
    if (Packet_GetOpcode(p) != MSG_HANDSHAKE)
    {
        LISSANDRA_LOG_ERROR("HANDSHAKE: recibido opcode no esperado %hu", Packet_GetOpcode(p));
        Packet_Destroy(p); /// agregado, libero memoria
        return;

    }

    /// Ariel: aca copie lo que habia en el manejador anterior de Handshake ya que no lo van a usar como un handler
    uint8_t id;
    Packet_Read(p, &id);

    //----Recibo un handshake del cliente para ver si es una memoria
    if (id != MEMORIA)
    {
        LISSANDRA_LOG_ERROR("Se conecto un desconocido! (id %d)", id);
        /// agregado, libero memoria
        Packet_Destroy(p);
        /// agregado, desconecto al desconocido
        Socket_Destroy(memoriaNueva);
        return;
    }

    LISSANDRA_LOG_INFO("Se conecto una memoria en el socket: %d\n", memoriaNueva->_impl.Handle);

    //TODO: deberia ser que si esta funcion me retorna "success", sigue, y sino hace un return
    //Ariel, si lees este mensaje, como no entiendo como manejas esto del handshake, no se si deberia
    //estar usando esta funcion aca directamente o en realidad esta funcion se invoca de otra manera.
    //Si se usa de otra manera, por favor mostrame como. Gracias!!! :D

    ///Esto es para cuando usan el EventDispatcher_Loop, el solito llama a los handler al recibir un paquete
    ///Pero como van a atender las memorias mediante hilos necesitan hacer algo de codigo custom. Les dejo un esqueleto
    ///más abajo (ver atender_memoria)
    ///HandleHandshake(memoriaNueva, p);

    Packet_Destroy(p);

    //Le envia a la memoria el TAMANIO_VALUE
    //Ariel: para paquetes conviene que usen los tipos de tamaño fijo (stdint.h)
    // en este caso al ser un tamaño yo uso un entero no signado de 32 bits
    /*int*/ uint32_t tamanioValue = confLFS->TAMANIO_VALUE;
    char* puntoMontaje = confLFS->PUNTO_MONTAJE;
    //A este Packet_Create en vez de un 2 habría que pasarle un opcode, pero como no se cual ponerle
    //le pase cualquiera para ver como funcionaba. Creo que hay que agregar uno nuevo en Opcodes.h
    //que se podría llamar "MSG_TAM_VALUE"
    /// Ariel: Esto está perfecto

    //Pero como no se si lo que estoy haciendo esta bien porque no entiendo nada, voy a esperar a que
    //Ariel lea esto y lo corrija :D Con mucho amor, Denise
    p = Packet_Create(MSG_HANDSHAKE_RESPUESTA, 20 /*ya que el tamaño es conocido (4 bytes) lo inicializo*/);
    Packet_Append(p, tamanioValue);
    Packet_Append(p, puntoMontaje);
    Socket_SendPacket(memoriaNueva, p);
    Packet_Destroy(p);

    //----Creo un hilo para cada memoria que se me conecta
    /* TODO: cada vez que una memoria se conecta, hilo_atender_memoria es sobreescrito
     * Deberiamos acordarnos de todas las memorias para hacer un pthread_join
     */
    pthread_create(&hilo_atender_memoria, NULL, atender_memoria, memoriaNueva);
}

void iniciar_servidor(void)
{
    /* -- codigo de inicializacion -- */
    //----Creo socket de LFS, hago el bind y comienzo a escuchar
    SocketOpts opts =
    {
        .SocketMode = SOCKET_SERVER,
        .ServiceOrPort = confLFS->PUERTO_ESCUCHA,
        .HostName = NULL,

        // cuando una memoria conecte, llamar a memoria_conectar
        .SocketOnAcceptClient = memoria_conectar,
    };
    sock_LFS = Socket_Create(&opts);

    //----Hago un log
    LISSANDRA_LOG_TRACE("Servidor LFS iniciado");

    /* -- main loop y threads -- */

    //Ariel: Monitorear socket de conexiones entrantes
    EventDispatcher_AddFDI(sock_LFS);
}

void mkdirRecursivo(char* path)
{

    char tmp[256];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, 0700);
            *p = '/';
        }
    mkdir(tmp, 0700);
}

bool existeArchivo(char* path)
{

    FILE* archi = fopen(path, "r");
    if (archi != NULL)
    {
        fclose(archi);
        return true;
    }
    else
    {
        return false;
    }
}

bool existeDir(char* pathDir)
{

    struct stat st = { 0 };
    if (stat(pathDir, &st) == -1)
    { //Si existe devuelve 0

        return false;

    }
    else
    {

        return true;

    }

}

char* generarPathTabla(char* nombreTabla)
{

    LISSANDRA_LOG_INFO("Generando el path de la tabla");
    char* pathAbsTabla = string_new();
    string_append(&pathAbsTabla, confLFS->PUNTO_MONTAJE);
    string_append(&pathAbsTabla, "Tables");
    if (!string_starts_with(nombreTabla, "/")) string_append(&pathAbsTabla, "/");
    string_append(&pathAbsTabla, nombreTabla);

    return pathAbsTabla;

}

int buscarBloqueLibre()
{

    int bloqueLibre;

    for (bloqueLibre = 0;
         bitarray_test_bit(bitArray, bloqueLibre) && bloqueLibre < confLFS->CANTIDAD_BLOQUES; bloqueLibre++)
    {

    }
    if (bloqueLibre >= confLFS->CANTIDAD_BLOQUES)
    {
        return -1;
    }

    return bloqueLibre;

}

void escribirValorBitarray(bool valor, int pos)
{

    if (valor)
        bitarray_set_bit(bitArray, pos);
    else
        bitarray_clean_bit(bitArray, pos);

    FILE* bitmap = fopen(pathMetadataBitarray, "w");
    fwrite(bitArray->bitarray, bitArray->size, 1, bitmap);
    fclose(bitmap);

}

t_describe* get_table_metadata(char* path, char* tabla)
{

    t_config* contenido = config_create(path);
    char* consistency = string_duplicate(config_get_string_value(contenido, "CONSISTENCY"));
    int partitions = config_get_int_value(contenido, "PARTITIONS");
    int compaction_time = config_get_int_value(contenido, "COMPACTION_TIME");
    config_destroy(contenido);

    t_describe* infoMetadata = malloc(sizeof(t_describe));
    infoMetadata->table = tabla;
    infoMetadata->consistency = consistency;
    infoMetadata->partitions = partitions;
    infoMetadata->compaction_time = compaction_time;

    //Pruebas Brenda/Denise desde ACA
    /*
    printf("Estoy en get_table_metadata\n");
    printf("Tabla: %s\n", infoMetadata->table);
    printf("Consistencia: %s\n", infoMetadata->consistency);
    printf("Particiones: %d\n", infoMetadata->partitions);
    printf("Tiempo: %d\n", infoMetadata->compaction_time);
     */
    //HASTA ACA

    return infoMetadata;

}

int traverse(char* fn, t_list* lista, char* tabla)
{
    DIR* dir;
    struct dirent* entry;
    char* path;
    struct stat info;
    t_describe* informacion;

    if ((dir = opendir(fn)) == NULL)
    {
        printf("ERROR: La ruta especificada es invalida\n");
        return -1;
    }
    else
    {
        while ((entry = readdir(dir)) != NULL)
        {
            path = string_new();
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                string_append(&path, fn);
                string_append(&path, "/");
                string_append(&path, entry->d_name);
                if (stat(path, &info) != 0)
                {
                    LISSANDRA_LOG_ERROR("Error stat() en %s", path);
                    return -1;
                }
                else
                {
                    if (S_ISDIR(info.st_mode))
                    {
                        int resultado = traverse(path, lista, entry->d_name);
                        if (resultado == -1)
                        {
                            return -1;
                        }
                    }
                    else
                    {
                        if (S_ISREG(info.st_mode))
                        {
                            if (strcmp(entry->d_name, "Metadata.bin") == 0)
                            {
                                informacion = get_table_metadata(path, tabla);
                                list_add(lista, informacion);
                                break;
                            }
                        }

                    }

                }

            }

        }

        closedir(dir);
        return 0;

    }
}

bool dirIsEmpty(char* path)
{

    int n = 0;
    DIR* dir;
    struct dirent* entry;

    dir = opendir(path);
    while ((entry = readdir(dir)) != NULL)
    {
        if (++n > 2)
            break;
    }
    closedir(dir);
    if (n <= 2) //Directorio vacio
        return true;
    else
        return false;
}

bool hayDump(char* nombreTabla)
{
    t_elem_memtable* elemento = memtable_get(nombreTabla);
    if (elemento == NULL)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool is_any(char* nombreArchivo)
{

    Vector strings = string_split(nombreArchivo, ".");
    char** string_1 = Vector_at(&strings, 0);
    char** string_2 = Vector_at(&strings, 1);

    char* string1 = (char*) string_1[0];
    char* string2 = (char*) string_2[0];

    if (strcmp(string2, "tmpc") == 0)
    {
        return true;
    }
    else
    {
        if (strcmp(string2, "tmp") == 0)
        {
            return true;
        }
        else
        {
            if (strcmp(string2, "bin") == 0)
            {
                if (strcmp(string1, "Metadata") == 0)
                {
                    return false;
                }
                else
                {
                    return true;
                }
            }
        }
    }

}

char* generarPathArchivo(char* nombreTabla, char* nombreArchivo)
{

    char* path = string_new();
    string_append(&path, confLFS->PUNTO_MONTAJE);
    string_append(&path, "Tables");
    string_append(&path, "/");
    string_append(&path, nombreTabla);
    string_append(&path, "/");
    string_append(&path, nombreArchivo);

    return path;

}

void borrarArchivo(char* nombreTabla, char* nombreArchivo)
{

    char* pathAbsoluto = generarPathArchivo(nombreTabla, nombreArchivo);

    t_config* data = config_create(pathAbsoluto);
    Vector bloques = config_get_array_value(data, "BLOCKS");
    size_t cantElementos = Vector_size(&bloques);

    int j = 0;
    t_config* elemento;

    while (j < cantElementos)
    {
        elemento = Vector_at(&bloques, j);
        escribirValorBitarray(0, atoi(elemento->path));
        j++;
    }

    unlink(pathAbsoluto);
    config_destroy(data);
    //config_destroy(elemento);
    free(pathAbsoluto);

}

int traverse_to_drop(char* fn, char* nombreTabla)
{
    DIR* dir;
    struct dirent* entry;
    struct stat info;
    char* path;

    if ((dir = opendir(fn)) == NULL)
    {
        printf("ERROR: La ruta especificada es invalida\n");
        return -1;
    }
    else
    {
        while ((entry = readdir(dir)) != NULL)
        {
            path = string_new();
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                string_append(&path, fn);
                string_append(&path, "/");
                string_append(&path, entry->d_name);
                if (stat(path, &info) != 0)
                {
                    LISSANDRA_LOG_ERROR("Error stat() en %s", path);
                    return -1;
                }
                else
                {
                    if (S_ISREG(info.st_mode))
                    {
                        if (is_any(entry->d_name))
                        {
                            borrarArchivo(nombreTabla, entry->d_name);
                        }
                        else
                        {
                            if (strcmp(entry->d_name, "Metadata.bin") == 0)
                            {
                                char* path_metadata = generarPathArchivo(nombreTabla, entry->d_name);
                                unlink(path_metadata);
                            }
                        }
                    }
                }
            }


        }
    }

    closedir(dir);
    return 0;

}

