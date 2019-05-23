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
    Socket* s = socketMemoria;
    (void) s;

    while (ProcessRunning)
    {
        //hacer cosas con la memoria
        //para agregar un pedido:
        //Ariel: ejemplo de uso de cola bloqueada
        //Ariel de nuevo: Creo que esto habria que hacerlo en Handlers.c (los que se llaman cada vez que recibimos un paquete)
        /*
        t_pedido* pedido = malloc(sizeof(t_pedido));
        pedido->id = SELECT;
        pedido->nombreTabla = "pepito";

        LockedQueue_Add(lista_pedidos, pedido);
        */
    }

    return NULL;
}

void memoria_conectar(Socket* fs, Socket* memoriaNueva)
{
    (void) fs;

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

    // inicializar lista de pedidos
    lista_pedidos = LockedQueue_Create();

    // inicializar semaforo en 0
    sem_init(&sem_pedido, 0 /*no utilizamos compartir con proceso*/, 0);

    //----Hago un log
    LISSANDRA_LOG_TRACE("Servidor LFS iniciado");

    /* -- main loop y threads -- */

    //----Creo un hilo para atender los pedidos que me van a hacer las memorias
    pthread_t hilo_atender_pedidos;
    pthread_create(&hilo_atender_pedidos, NULL, atender_pedidos, NULL);

    //Ariel: Monitorear socket de conexiones entrantes
    EventDispatcher_AddFDI(sock_LFS);

    //----En loop infinito acepto los clientes (memorias)
    EventDispatcher_Loop();

    /* --codigo de finalizacion-- */
    pthread_join(hilo_atender_pedidos, NULL);
    // TODO pthread_join de los hilos de memoria (si hubo)

    // cuando finaliza el proceso, limpiar las variables utilizadas
    sem_destroy(&sem_pedido);
    LockedQueue_Destroy(lista_pedidos, Free);
    Socket_Destroy(sock_LFS);
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
    string_append(&pathAbsTabla, "Tablas");
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
