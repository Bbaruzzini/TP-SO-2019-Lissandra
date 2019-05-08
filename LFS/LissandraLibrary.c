//
// Created by Denise on 26/04/2019.
//

#include "LissandraLibrary.h"

int server_socket(uint16_t port)
{
    int sock_fd, optval = 1;
    struct sockaddr_in servername;

    /* Create the socket. */
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    /* Set socket options. */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) == -1) {
        perror("setsockopt");
        return -2;
    }

    /* Fill ip / port info. */
    servername.sin_family = AF_INET;
    servername.sin_addr.s_addr = htonl(INADDR_ANY);
    servername.sin_port = htons(port);

    /* Give the socket a name. */
    if (bind(sock_fd, (struct sockaddr *) &servername, sizeof servername) < 0) {
        perror("bind");
        return -3;
    }

    /* Listen to incoming connections. */
    if (listen(sock_fd, 1) < 0) {
        perror("listen");
        return -4;
    }

    return sock_fd;
}


int client_socket(char *ip, uint16_t port)
{
    printf("Entre a client_socket\n");

    int sock_fd;
    struct sockaddr_in servername;

    /* Create the socket. */
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    printf("Cree el socket\n");

    /* Fill server ip / port info. */
    servername.sin_family = AF_INET;
    servername.sin_addr.s_addr = inet_addr(ip);
    servername.sin_port = htons(port);
    memset(&(servername.sin_zero), 0, 8);

    printf("Llego a este punto\n");

    /* Connect to the server. */
    if (connect(sock_fd, (struct sockaddr *) &servername, sizeof servername) < 0) {
        perror("connect");
        return -2;
    }

    printf("Hice el connect, salgo de client_socket\n");
    return sock_fd;
}


int accept_connection(int sock_fd)
{
    struct sockaddr_in clientname;
    size_t size = sizeof clientname;

    int new_fd = accept(sock_fd, (struct sockaddr *) &clientname, (socklen_t *) &size);
    if (new_fd < 0) {
        perror("accept");
        return -1;
    }

    return new_fd;
}

t_header* deserializar_header(void* buffer){

    t_header* header = malloc(sizeof(t_header));

    memcpy(&(header->id), buffer, sizeof(int));
    memcpy(&(header->size), buffer + sizeof(int), sizeof(int));

    free(buffer);

    return header;
}

int recibir_handshake(int fd){

    void* buffer = malloc(2 * sizeof(int));

    int resultado = recv(fd, buffer, 2 * sizeof(int), MSG_WAITALL);

    if (resultado <= 0) {

        perror("Error al recibir el handshake");
        return resultado;

    }

    t_header* header = deserializar_header(buffer);

    int handshake_id = header->id;

    free(header);

    return handshake_id;
}

t_pedido* obtener_pedido(){

    t_pedido* pedido = NULL;

    pedido = list_remove(lista_pedidos, 0);

    return pedido;

}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//ATENCION!!!!!!!!!!!!! BRENDAAAAA DENISEEEEE --> t_pedido hay que armarlo nosotras!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void* atender_pedidos(){

    while(1){

        //----Utilizo un semáforo (originalmente iniciado en 0) para saber cuándo hay pedidos en la lista de pedidos y atenderlos.
        sem_wait(&sem_pedido);
        //----Bloqueo este semaforo para que ningun otro hilo toque la lista de pedidos (que sería sólo para añadir pedidos)
        pthread_mutex_lock(&mutex_lista_pedidos);
        //----Busco el pedido que hizo esta memoria
        t_pedido* pedido = obtener_pedido();
        //----Desbloqueo este semaforo para que otros hilos ya puedan usar la lista de pedidos (sólo para añadir pedidos)
        pthread_mutex_unlock(&mutex_lista_pedidos);
        //----Segun el id del pedido, ejecuto el procedimiento correspondiente
        switch(pedido->id){

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
        free(pedido);

    }

}

void* iniciar_servidor(){

//----Creo socket de LFS, hago el bind y comienzo a escuchar
    sock_LFS = server_socket(confLFS->PUERTO_ESCUCHA);

    //----Hago un log
    LISSANDRA_LOG_TRACE("Servidor LFS iniciado");

    //----Creo variable para el socket del cliente
    int sock_memoria;

    //----Creo un hilo para atender los pedidos que me van a hacer las memorias
    pthread_create(&hilo_atender_pedidos, NULL, atender_pedidos, NULL);

    //----En loop infinito acepto los clientes (memorias)
    while (1) {

        //----Acepto conexion de un cliente
        sock_memoria = accept_connection(sock_LFS);

        if (sock_memoria <= 0){

            printf("Error en el accept");

            continue;

        }

        //----Recibo un handshake del cliente para ver si es una memoria
        int id = recibir_handshake(sock_memoria);

        if(id != MEMORIA){

            printf("Se conecto un desconocido");

            continue;

        }

        printf("Se conecto una memoria en el socket: %d\n", sock_memoria);

        //----Creo un hilo para cada memoria que se me conecta
        pthread_create(&hilo_atender_memoria, NULL, atender_memoria, (void *) sock_memoria);


    }

    return EXIT_SUCCESS;

}
