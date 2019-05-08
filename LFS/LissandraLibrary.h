//
// Created by Denise on 26/04/2019.
//

#ifndef LISSANDRA_LISSANDRALIBRARY_H
#define LISSANDRA_LISSANDRALIBRARY_H

//Linkear bibliotecas!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include "Lissandra.h"
#include <libcommons/list.h>


#define MEMORIA 2

typedef struct t_header{
    int id;
    int size;
} __attribute__((packed)) t_header;

//Variables
int sock_LFS;

t_list* lista_pedidos;


sem_t sem_pedido; //Cuando hagamos copy paste, cambiar el semaforo "pedido" por "sem_pedido"
pthread_mutex_t mutex_lista_pedidos;

pthread_t hilo_atender_memoria;
pthread_t hilo_atender_pedidos;





/*
 * Crea, vincula y escucha un socket desde un puerto determinado.
 */
int server_socket(uint16_t port);

/*
 * Crea y conecta a una ip:puerto determinado.
 */
int client_socket(char* ip, uint16_t port);

/*
 * Acepta la conexion de un socket.
 */
int accept_connection(int sock_fd);


void* iniciar_servidor();

int recibir_handshake(int fd);
t_header* deserializar_header(void* buffer);
t_pedido* obtener_pedido();

#endif //LISSANDRA_LISSANDRALIBRARY_H
