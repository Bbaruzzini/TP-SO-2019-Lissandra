//
// Created by Denise on 26/04/2019.
//

#include "LissandraLibrary.h"
#include "Config.h"
#include "FileSystem.h"
#include "Memtable.h"
#include <Consistency.h>
#include <Console.h>
#include <ConsoleInput.h>
#include <dirent.h>
#include <EventDispatcher.h>
#include <fcntl.h>
#include <File.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <LockedQueue.h>
#include <Opcodes.h>
#include <Packet.h>
#include <semaphore.h>
#include <Socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <Threads.h>
#include <unistd.h>
#include <Timer.h>

//Variables
//static hace que las variables no se puedan referenciar desde otro .c utilizando 'extern'
//si lo desean cambiar, quitenlo
static Socket* sock_LFS = NULL;

static LockedQueue* lista_pedidos = NULL;

static sem_t sem_pedido = { 0 }; //Cuando hagamos copy paste, cambiar el semaforo "pedido" por "sem_pedido"

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

        Free(pedido->path);
*/
        Free(pedido);
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
    uint32_t const tamanioValue = confLFS.TAMANIO_VALUE;
    char const* const puntoMontaje = confLFS.PUNTO_MONTAJE;
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
    Threads_CreateDetached(atender_memoria, memoriaNueva);
}

void iniciar_servidor(void)
{
    /* -- codigo de inicializacion -- */
    //----Creo socket de LFS, hago el bind y comienzo a escuchar
    SocketOpts opts =
    {
        .SocketMode = SOCKET_SERVER,
        .ServiceOrPort = confLFS.PUERTO_ESCUCHA,
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

void mkdirRecursivo(char const* path)
{
    char tmp[256];
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char* p = tmp + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }

    mkdir(tmp, 0700);
}

bool existeArchivo(char const* path)
{
    File* file = file_open(path, F_OPEN_READ);
    bool res = file_is_open(file);

    file_close(file);
    return res;
}

bool existeDir(char const* pathDir)
{
    struct stat st = { 0 };

    //Si existe devuelve 0
    return stat(pathDir, &st) != -1;
}

void generarPathTabla(char* nombreTabla, char* buf)
{
    LISSANDRA_LOG_INFO("Generando el path de la tabla");

    //Como los nombres de las tablas deben estar en uppercase, primero me aseguro de que así sea y luego genero el path de esa tabla
    string_to_upper(nombreTabla);
    snprintf(buf, PATH_MAX, "%sTables/%s", confLFS.PUNTO_MONTAJE, nombreTabla);
}

bool buscarBloqueLibre(size_t* bloqueLibre)
{
    size_t i = 0;
    for (; bitarray_test_bit(bitArray, i) && i < confLFS.CANTIDAD_BLOQUES; ++i);

    if (i >= confLFS.CANTIDAD_BLOQUES)
        return false;

    *bloqueLibre = i;

    // marca el bloque como ocupado
    escribirValorBitarray(true, i);
    return true;
}

void generarPathBloque(size_t numBloque, char* buf)
{
    snprintf(buf, PATH_MAX, "%sBloques/%d.bin", confLFS.PUNTO_MONTAJE, numBloque);
}

void escribirValorBitarray(bool valor, size_t pos)
{
    if (valor)
        bitarray_set_bit(bitArray, pos);
    else
        bitarray_clean_bit(bitArray, pos);
}

t_describe* get_table_metadata(char const* path, char const* tabla)
{
    t_config* contenido = config_create(path);
    char const* consistency = config_get_string_value(contenido, "CONSISTENCY");
    CriteriaType ct;
    if (!CriteriaFromString(consistency, &ct)) // error!
        return NULL;
    uint16_t partitions = config_get_int_value(contenido, "PARTITIONS");
    uint32_t compaction_time = config_get_long_value(contenido, "COMPACTION_TIME");
    config_destroy(contenido);

    t_describe* infoMetadata = Malloc(sizeof(t_describe));
    snprintf(infoMetadata->table, NAME_MAX + 1, "%s", tabla);
    infoMetadata->consistency = (uint8_t) ct;
    infoMetadata->partitions = partitions;
    infoMetadata->compaction_time = compaction_time;

    //Pruebas Brenda/Denise desde ACA
    /*
    printf("Estoy en get_table_metadata\n");
    printf("Tabla: %s\n", infoMetadata->table);
    printf("Consistencia: %d\n", infoMetadata->consistency);
    printf("Particiones: %d\n", infoMetadata->partitions);
    printf("Tiempo: %d\n", infoMetadata->compaction_time);
    */
    //HASTA ACA

    return infoMetadata;
}

int traverse(char const* fn, t_list* lista, char const* tabla)
{
    DIR* dir;
    if (!(dir = opendir(fn)))
    {
        printf("ERROR: La ruta especificada es invalida\n");
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        char path[PATH_MAX];
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            struct stat info;

            snprintf(path, PATH_MAX, "%s/%s", fn, entry->d_name);
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
                        return -1;
                }
                else
                {
                    if (S_ISREG(info.st_mode))
                    {
                        if (strcmp(entry->d_name, "Metadata.bin") == 0)
                        {
                            list_add(lista, get_table_metadata(path, tabla));
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

bool dirIsEmpty(char const* path)
{
    int n = 0;
    DIR* dir;
    struct dirent* entry;

    dir = opendir(path);
    while ((entry = readdir(dir)))
    {
        if (++n > 2)
            break;
    }
    closedir(dir);

    //Directorio vacio contiene solo 2 entradas: "." y ".."
    return n <= 2;
}

bool is_any(char const* nombreArchivo)
{
    Vector strings = string_split(nombreArchivo, ".");
    char** fileName = Vector_data(&strings);

    char const* name = fileName[0];
    char const* ext = fileName[1];

    // tmpc || tmp || (bin && !Metadata)
    bool res = false;
    if (0 == strcmp(ext, "tmpc") || 0 == strcmp(ext, "tmp") ||
        (0 == strcmp(ext, "bin") && 0 != strcmp(name, "Metadata")))
        res = true;

    Vector_Destruct(&strings);
    return res;
}

void generarPathArchivo(char const* nombreTabla, char const* nombreArchivo, char* buf)
{
    snprintf(buf, PATH_MAX, "%sTables/%s/%s", confLFS.PUNTO_MONTAJE, nombreTabla, nombreArchivo);
}

void borrarArchivo(char const* nombreTabla, char const* nombreArchivo)
{
    char pathAbsoluto[PATH_MAX];
    generarPathArchivo(nombreTabla, nombreArchivo, pathAbsoluto);

    t_config* data = config_create(pathAbsoluto);

    /// Ariel: config_get_array_value devuelve un Vector de char*
    /// Los chars se liberan automaticamente cuando se llama a vector_destruct
    Vector bloques = config_get_array_value(data, "BLOCKS");
    size_t cantElementos = Vector_size(&bloques);

    size_t j = 0;
    t_config* elemento;

    while (j < cantElementos)
    {
        ///Ariel: problema: estan asumiendo que el elemento de bloques es un t_config
        /// vector maneja una abstraccion de "puntero a elemento" por lo cual el verdadero tipo de lo que les tira
        /// vector_at es char* * (puntero al elemento, que es un char*)

        elemento = Vector_at(&bloques, j);
        escribirValorBitarray(false, atoi(elemento->path));
        ++j;
    }

    unlink(pathAbsoluto);
    config_destroy(data);
    //config_destroy(elemento);
}

int traverse_to_drop(char const* fn, char const* nombreTabla)
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
                                char path_metadata[PATH_MAX];
                                generarPathArchivo(nombreTabla, entry->d_name, path_metadata);
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

uint16_t get_particion(uint16_t particiones, uint16_t key)
{
    //key mod particiones de la tabla
    return key % particiones;
}

void generarPathParticion(uint16_t particion, char* pathTabla, char* pathParticion)
{
    snprintf(pathParticion, PATH_MAX, "%s/%d.bin", pathTabla, particion);
}

t_registro* get_biggest_timestamp(char const* contenido, uint16_t key)
{
    Vector registros = string_split(contenido, "\n");

    t_registro* resultado = NULL;
    char** const tokens = Vector_data(&registros);
    for (size_t i = 0; i < Vector_size(&registros); ++i)
    {
        char* const registro = tokens[i];

        uint64_t timestamp;
        uint16_t keyToken;
        char* value;

        {
            Vector campos = string_split(registro, ";");
            // timestamp;key;value
            char** const fields = Vector_data(&campos);
            timestamp = strtoull(fields[0], NULL, 10);
            if (!ValidateKey(fields[1], &keyToken))
                break;

            value = string_duplicate(fields[2]);
            Vector_Destruct(&campos);
        }

        if (key == keyToken)
        {
            if (!resultado)
            {
                resultado = Malloc(sizeof(t_registro));

                resultado->key = keyToken;
                resultado->value = string_duplicate(value);
                resultado->timestamp = timestamp;
            }
            else if (resultado->timestamp < timestamp)
            {
                resultado->key = keyToken;

                Free(resultado->value);
                resultado->value = string_duplicate(value);

                resultado->timestamp = timestamp;
            }
        }
    }

    Vector_Destruct(&registros);
    return resultado;
}

t_registro* scanParticion(char const* pathParticion, uint16_t key)
{
    char* contenido = leerArchivoLFS(pathParticion);
    t_registro* registro = get_biggest_timestamp(contenido, key);
    Free(contenido);
    return registro;
}

t_registro* temporales_get_biggest_timestamp(char const* pathTabla, uint16_t key)
{
    DIR* dir;
    if (!(dir = opendir(pathTabla)))
    {
        printf("ERROR: La ruta especificada es invalida\n");
        return NULL;
    }

    t_registro* resultado = NULL;

    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        char path[PATH_MAX];
        if (*entry->d_name != '.')
        {
            struct stat info;
            snprintf(path, PATH_MAX, "%s/%s", pathTabla, entry->d_name);

            if (stat(path, &info) != 0)
            {
                LISSANDRA_LOG_SYSERROR("stat");
                closedir(dir);
                return NULL;
            }

            bool istmp = false;
            {
                Vector aux = string_split(path, ".");
                char** fileName = Vector_data(&aux);

                char const* ext = fileName[1];
                if (0 == strcmp(ext, "tmp") || 0 == strcmp(ext, "tmpc"))
                    istmp = true;

                Vector_Destruct(&aux);
            }

            if (!istmp)
                continue;

            char* contenido = leerArchivoLFS(path);
            t_registro* registroAux = get_biggest_timestamp(contenido, key);
            if (!resultado)
            {
                resultado = Malloc(sizeof(t_registro));
                resultado->key = registroAux->key;
                resultado->value = string_duplicate(registroAux->value);
                resultado->timestamp = registroAux->timestamp;
            }
            else if (resultado->timestamp < registroAux->timestamp)
            {
                resultado->key = registroAux->key;

                Free(resultado->value);
                resultado->value = string_duplicate(registroAux->value);

                resultado->timestamp = registroAux->timestamp;
            }
        }
    }

    closedir(dir);
    return resultado;
}

t_registro* get_newest(t_registro* particion, t_registro* temporales, t_registro* memtable)
{
    size_t num = 0;
    t_registro* registros[3];

    if (particion)
        registros[num++] = particion;

    if (temporales)
        registros[num++] = temporales;

    if (memtable)
        registros[num++] = memtable;

    t_registro* mayor = NULL;
    uint64_t timestampMayor = 0;
    for (size_t i = 0; i < num; ++i)
    {
        if (!mayor || registros[i]->timestamp > timestampMayor)
        {
            timestampMayor = registros[i]->timestamp;
            mayor = registros[i];
        }
    }

    return mayor;
}

char* leerArchivoLFS(const char* path)
{
    FILE* part = fopen(path, "r");
    if (!part)
    {
        LISSANDRA_LOG_ERROR("No se encontro el archivo en el File System");
        return NULL;
    }

    Vector bloques;
    size_t longitudArchivo;
    {
        t_config* datos = config_create(path);
        bloques = config_get_array_value(datos, "BLOCKS");
        longitudArchivo = config_get_long_value(datos, "SIZE");
        config_destroy(datos);
    }

    size_t const bloquesTotales = Vector_size(&bloques);

    char* contenido = Malloc(longitudArchivo);
    size_t offset = 0;

    char** const arrayBloques = Vector_data(&bloques);
    for (size_t i = 0; i < bloquesTotales; ++i)
    {
        size_t const numBloque = strtoul(arrayBloques[i], NULL, 10);

        char pathBloque[PATH_MAX];
        generarPathBloque(numBloque, pathBloque);

        int fd = open(pathBloque, O_RDONLY);
        if (fd == -1)
        {
            LISSANDRA_LOG_SYSERROR("open");
            exit(EXIT_FAILURE);
        }

        char* mapping = mmap(0, confLFS.TAMANIO_BLOQUES, PROT_READ, MAP_SHARED, fd, 0);
        if (mapping == MAP_FAILED)
        {
            LISSANDRA_LOG_SYSERROR("mmap");
            exit(EXIT_FAILURE);
        }

        memcpy(contenido + offset, mapping, confLFS.TAMANIO_BLOQUES);
        offset += confLFS.TAMANIO_BLOQUES;

        if (munmap(mapping, confLFS.TAMANIO_BLOQUES) == -1)
        {
            LISSANDRA_LOG_SYSERROR("munmap");
            exit(EXIT_FAILURE);
        }

        close(fd);
    }

    fclose(part);
    Vector_Destruct(&bloques);

    return contenido;
}

void escribirArchivoLFS(char const* path, void const* buf, size_t len)
{
    t_config* file = config_create(path);
    if (!file)
        return;

    size_t oldSize = config_get_long_value(file, "SIZE");
    Vector blocks = config_get_array_value(file, "BLOCKS");

    // el archivo ocupa actualmente N bloques, calcular espacio restante en el ultimo bloque
    size_t remaining = Vector_size(&blocks) * confLFS.TAMANIO_BLOQUES - oldSize;

    // calcular cuantos bloques nuevos se requieren
    size_t blocksNeeded = 0;
    if (len > remaining)
    {
        blocksNeeded = (len - remaining) / confLFS.TAMANIO_BLOQUES;
        if ((len - remaining) % confLFS.TAMANIO_BLOQUES)
            ++blocksNeeded;
    }

    // guardar bloques nuevos
    for (size_t i = 0; i < blocksNeeded; ++i)
    {
        size_t bloque;
        if (!buscarBloqueLibre(&bloque))
        {
            LISSANDRA_LOG_FATAL("No hay más bloques disponibles en el FS!");
            exit(EXIT_FAILURE);
        }

        char* blockNum = string_from_format("%u", bloque);
        Vector_push_back(&blocks, &blockNum);
    }

    // calcular nuevo tamaño, bytes
    char* newSize = string_from_format("%u", oldSize + len);

    // listo, ya el archivo tiene los bloques suficientes, escribamos bloque a bloque
    // comenzando por el ultimo viejo
    size_t writeLength = remaining;
    if (len < remaining)
        writeLength = len;

    char** const blockArray = Vector_data(&blocks);

    size_t offset = confLFS.TAMANIO_BLOQUES - remaining; // en donde debo pararme en el ultimo bloque viejo
    for (size_t i = oldSize / confLFS.TAMANIO_BLOQUES; i < Vector_size(&blocks); ++i)
    {
        size_t const blockNum = strtoul(blockArray[i], NULL, 10);

        char pathBloque[PATH_MAX];
        generarPathBloque(blockNum, pathBloque);

        int fd = open(pathBloque, O_RDWR);
        if (fd == -1)
        {
            LISSANDRA_LOG_SYSERROR("open");
            exit(EXIT_FAILURE);
        }

        char* mapping = mmap(NULL, confLFS.TAMANIO_BLOQUES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapping == MAP_FAILED)
        {
            LISSANDRA_LOG_SYSERROR("mmap");
            exit(EXIT_FAILURE);
        }

        memcpy(mapping + offset, buf, writeLength);
        buf = (uint8_t const*) buf + writeLength;
        len -= writeLength;

        munmap(mapping, confLFS.TAMANIO_BLOQUES);
        close(fd);

        // los proximos bloques estan vacios
        offset = 0;
        writeLength = confLFS.TAMANIO_BLOQUES;
        if (len < confLFS.TAMANIO_BLOQUES)
            writeLength = len;
    }

    config_set_value(file, "SIZE", newSize);
    Free(newSize);

    config_set_array_value(file, "BLOCKS", &blocks);
    Vector_Destruct(&blocks);

    config_save(file);
    config_destroy(file);
}

void crearArchivoLFS(char const* path, size_t block)
{
    FILE* temporal = fopen(path, "w");
    fprintf(temporal, "SIZE=0\n");
    fprintf(temporal, "BLOCKS=[%u]\n", block);
    fclose(temporal);
}
