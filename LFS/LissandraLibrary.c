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
#include <Opcodes.h>
#include <Packet.h>
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

static pthread_mutex_t bitarrayLock = PTHREAD_MUTEX_INITIALIZER;

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
    pthread_mutex_lock(&bitarrayLock);

    size_t i = 0;
    for (; bitarray_test_bit(bitArray, i) && i < confLFS.CANTIDAD_BLOQUES; ++i);

    if (i >= confLFS.CANTIDAD_BLOQUES)
    {
        pthread_mutex_unlock(&bitarrayLock);
        return false;
    }

    *bloqueLibre = i;

    // marca el bloque como ocupado
    bitarray_set_bit(bitArray, i);

    pthread_mutex_unlock(&bitarrayLock);
    return true;
}

void generarPathBloque(size_t numBloque, char* buf)
{
    snprintf(buf, PATH_MAX, "%sBloques/%d.bin", confLFS.PUNTO_MONTAJE, numBloque);
}

void escribirValorBitarray(bool valor, size_t pos)
{
    pthread_mutex_lock(&bitarrayLock);

    if (valor)
        bitarray_set_bit(bitArray, pos);
    else
        bitarray_clean_bit(bitArray, pos);

    pthread_mutex_unlock(&bitarrayLock);
}

bool get_table_metadata(char const* path, char const* tabla, t_describe* res)
{
    t_config* contenido = config_create(path);
    char const* consistency = config_get_string_value(contenido, "CONSISTENCY");
    CriteriaType ct;
    if (!CriteriaFromString(consistency, &ct)) // error!
        return false;

    uint16_t partitions = config_get_int_value(contenido, "PARTITIONS");
    uint32_t compaction_time = config_get_long_value(contenido, "COMPACTION_TIME");
    config_destroy(contenido);

    snprintf(res->table, NAME_MAX + 1, "%s", tabla);
    res->consistency = (uint8_t) ct;
    res->partitions = partitions;
    res->compaction_time = compaction_time;

    //Pruebas Brenda/Denise desde ACA
    /*
    printf("Estoy en get_table_metadata\n");
    printf("Tabla: %s\n", infoMetadata->table);
    printf("Consistencia: %d\n", infoMetadata->consistency);
    printf("Particiones: %d\n", infoMetadata->partitions);
    printf("Tiempo: %d\n", infoMetadata->compaction_time);
    */
    //HASTA ACA

    return true;
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
                else if (S_ISREG(info.st_mode))
                {
                    if (strcmp(entry->d_name, "Metadata.bin") == 0)
                    {
                        t_describe* desc = Malloc(sizeof(t_describe));
                        if (!get_table_metadata(path, tabla, desc))
                        {
                            Free(desc);
                            continue;
                        }

                        list_add(lista, desc);
                        break;
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

bool get_biggest_timestamp(char const* contenido, uint16_t key, t_registro* resultado)
{
    Vector registros = string_split(contenido, "\n");

    bool found = false;
    char** const tokens = Vector_data(&registros);
    for (size_t i = 0; i < Vector_size(&registros); ++i)
    {
        char* const registro = tokens[i];

        Vector campos = string_split(registro, ";");
        // timestamp;key;value
        char** const fields = Vector_data(&campos);
        uint64_t const timestamp = strtoull(fields[0], NULL, 10);

        uint16_t keyRegistro;
        if (!ValidateKey(fields[1], &keyRegistro))
        {
            Vector_Destruct(&campos);
            break;
        }

        if (key == keyRegistro)
        {
            char* const value = fields[2];
            if (!found)
            {
                found = true;

                resultado->key = keyRegistro;
                resultado->timestamp = timestamp;
                strncpy(resultado->value, value, confLFS.TAMANIO_VALUE + 1);
            }
            else if (resultado->timestamp < timestamp)
            {
                resultado->timestamp = timestamp;
                strncpy(resultado->value, value, confLFS.TAMANIO_VALUE + 1);
            }
        }

        Vector_Destruct(&campos);
    }

    Vector_Destruct(&registros);
    return found;
}

bool scanParticion(char const* pathParticion, uint16_t key, t_registro* registro)
{
    char* contenido = leerArchivoLFS(pathParticion);
    if (!contenido)
        return NULL;

    bool resultado = get_biggest_timestamp(contenido, key, registro);
    Free(contenido);
    return resultado;
}

bool temporales_get_biggest_timestamp(char const* pathTabla, uint16_t key, t_registro* registro)
{
    DIR* dir;
    if (!(dir = opendir(pathTabla)))
    {
        printf("ERROR: La ruta especificada es invalida\n");
        return NULL;
    }

    bool foundAny = false;
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
            if (!contenido)
                continue;

            size_t const tamRegistro = sizeof(t_registro) + confLFS.TAMANIO_VALUE + 1;
            t_registro* registroTemp = Malloc(tamRegistro);
            if (!get_biggest_timestamp(contenido, key, registroTemp))
            {
                Free(registroTemp);
                Free(contenido);
                continue;
            }

            if (!foundAny)
            {
                foundAny = true;

                memcpy(registro, registroTemp, tamRegistro);
            }
            else if (registro->timestamp < registroTemp->timestamp)
            {
                registro->timestamp = registroTemp->timestamp;
                strncpy(registro->value, registroTemp->value, confLFS.TAMANIO_VALUE + 1);
            }

            Free(registroTemp);
            Free(contenido);
        }
    }

    closedir(dir);
    return foundAny;
}

t_registro const* get_newest(t_registro const* particion, t_registro const* temporales, t_registro const* memtable)
{
    size_t num = 0;
    t_registro const* registros[3];

    if (particion)
        registros[num++] = particion;

    if (temporales)
        registros[num++] = temporales;

    if (memtable)
        registros[num++] = memtable;

    t_registro const* mayor = NULL;
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
    Vector bloques;
    size_t bytesLeft;
    {
        t_config* file = config_create(path);
        if (!file)
        {
            LISSANDRA_LOG_ERROR("No se encontro el archivo en el File System");
            return NULL;
        }

        bloques = config_get_array_value(file, "BLOCKS");
        bytesLeft = config_get_long_value(file, "SIZE");
        config_destroy(file);
    }

    size_t const longitudArchivo = bytesLeft;

    size_t const bloquesTotales = Vector_size(&bloques);

    // cuanto leer
    size_t readLen = confLFS.TAMANIO_BLOQUES;
    if (longitudArchivo < readLen)
        readLen = longitudArchivo;

    char* const contenido = Malloc(longitudArchivo + 1);
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

        char* mapping = mmap(0, confLFS.TAMANIO_BLOQUES, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED)
        {
            LISSANDRA_LOG_SYSERROR("mmap");
            exit(EXIT_FAILURE);
        }

        memcpy(contenido + offset, mapping, readLen);
        offset += readLen;
        bytesLeft -= readLen;

        if (munmap(mapping, confLFS.TAMANIO_BLOQUES) == -1)
        {
            LISSANDRA_LOG_SYSERROR("munmap");
            exit(EXIT_FAILURE);
        }

        close(fd);

        readLen = confLFS.TAMANIO_BLOQUES;
        if (bytesLeft < readLen)
            readLen = bytesLeft;
    }

    Vector_Destruct(&bloques);

    contenido[longitudArchivo] = '\0';
    return contenido;
}

static bool _pedirBloquesNuevos(char const* path, Vector* bloques, size_t n)
{
    Vector nuevos;
    Vector_Construct(&nuevos, sizeof(size_t), NULL, n);
    for (size_t i = 0; i < n; ++i)
    {
        size_t bloque;
        if (!buscarBloqueLibre(&bloque))
        {
            LISSANDRA_LOG_ERROR("No hay más bloques disponibles para guardar el archivo %s! Se perderán datos", path);

            // liberar los que pude pedir hasta ahora
            for (size_t j = 0; j < Vector_size(&nuevos); ++j)
            {
                size_t* const b = Vector_at(&nuevos, j);
                escribirValorBitarray(false, *b);
            }

            Vector_Destruct(&nuevos);
            return false;
        }

        Vector_push_back(&nuevos, &bloque);
    }

    for (size_t i = 0; i < Vector_size(&nuevos); ++i)
    {
        size_t* const bloque = Vector_at(&nuevos, i);

        char* blockNum = string_from_format("%u", *bloque);
        Vector_push_back(bloques, &blockNum);
    }

    Vector_Destruct(&nuevos);
    return true;
}

static inline void _escribirBloque(size_t block, char const* buf, size_t len)
{
    char pathBloque[PATH_MAX];
    generarPathBloque(block, pathBloque);

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

    memcpy(mapping, buf, len);

    munmap(mapping, confLFS.TAMANIO_BLOQUES);
    close(fd);
}

void escribirArchivoLFS(char const* path, char const* buf, size_t len)
{
    t_config* file = config_create(path);
    if (!file)
        return;

    size_t bloquesTotales = len / confLFS.TAMANIO_BLOQUES;
    if (len % confLFS.TAMANIO_BLOQUES)
        ++bloquesTotales;

    Vector blocks = config_get_array_value(file, "BLOCKS");

    size_t const bloquesActuales = Vector_size(&blocks);
    if (bloquesActuales < bloquesTotales)
    {
        // calcular cuantos bloques nuevos se requieren
        size_t const bloquesNecesarios = bloquesTotales - bloquesActuales;

        // pedir bloques nuevos
        if (!_pedirBloquesNuevos(path, &blocks, bloquesNecesarios))
        {
            Vector_Destruct(&blocks);
            config_destroy(file);
            return;
        }
    }
    else
    {
        size_t const bloquesSobrantes = bloquesActuales - bloquesTotales;

        // descartar los ultimos
        for (size_t i = 0; i < bloquesSobrantes; ++i)
        {
            char** const b = Vector_back(&blocks);
            size_t const blockNum = strtoull(*b, NULL, 10);

            // liberar bloque
            escribirValorBitarray(false, blockNum);
            Vector_pop_back(&blocks);
        }
    }

    char* newSize = string_from_format("%u", len);

    // listo, ya el archivo tiene los bloques suficientes, escribamos bloque a bloque
    char** const blockArray = Vector_data(&blocks);
    size_t i = 0;
    while (i < len / confLFS.TAMANIO_BLOQUES)
    {
        size_t const blockNum = strtoul(blockArray[i++], NULL, 10);

        _escribirBloque(blockNum, buf, confLFS.TAMANIO_BLOQUES);
        buf += confLFS.TAMANIO_BLOQUES;
    }

    // ultimo bloque
    if (len % confLFS.TAMANIO_BLOQUES)
        _escribirBloque(strtoul(blockArray[i], NULL, 10), buf, len % confLFS.TAMANIO_BLOQUES);

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
