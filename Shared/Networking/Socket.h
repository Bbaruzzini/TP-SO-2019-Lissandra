
#ifndef Socket_h__
#define Socket_h__

#include "FileDescInterface.h"
#include "IPAddress.h"
#include "MessageBuffer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Socket Socket;
typedef struct Packet Packet;

typedef struct Socket
{
    FDI _impl;

    IPAddress Address;
    bool Blocking;

    MessageBuffer RecvBuffer;
    MessageBuffer SendBuffer;

    MessageBuffer HeaderBuffer;
    MessageBuffer PacketBuffer;
} Socket;

typedef struct
{
    char const* HostName;
    char const* ServiceOrPort;

    enum SocketMode
    {
        SOCKET_CLIENT,
        SOCKET_SERVER
    } SocketMode;
} SocketOpts;

#define BACKLOG 10
#define READ_BLOCK_SIZE 4096

/*
 * Socket_Create: crea un nuevo Socket
 * Parámetros: estructura opts, con las opciones del mismo
 *  - HostName: string - ip o nombre del remoto, para servidores se puede pasar NULL
 *  - ServiceOrPort: string - puerto al que conectar, o en el cual aceptar conexiones si soy servidor
 *      También puede ponerse como string uno de los servicios registados en /etc/services
 *  - SocketMode: enum - O bien SOCKET_CLIENT o bien SOCKET_SERVER, dependiendo de lo que querramos
 *      el modo SOCKET_CLIENT inicia la conexión al remoto
 *      el modo SOCKET_SERVER acepta conexiones entrantes en el puerto pasado por parámetro
 */
Socket* Socket_Create(SocketOpts const* opts);

/*
 * Socket_IsBlocking: devuelve true si el socket es bloqueante (por defecto)
 */
bool Socket_IsBlocking(Socket const* s);

/*
 * Socket_SetBlocking: cambia el socket a bloqueante o no bloqueante
 */
void Socket_SetBlocking(Socket* s, bool block);

/*
 * Socket_SendPacket: envía un paquete serializado al host conectado
 */
void Socket_SendPacket(Socket* s, Packet const* packet);

/*
 * Socket_RecvPacket: recibe y procesa un paquete serializado. Esta operación es solo para socket bloqueantes
 * Devuelve true si fue exitoso, o falso si hubo un error, en caso de devolver falso el socket ya no es válido
 */
bool Socket_RecvPacket(Socket* s);

/*
 * Socket_Destroy: destructor, llamar luego de terminar de operar con el socket para liberar memoria
 */
void Socket_Destroy(void* elem);

#endif //Socket_h__
