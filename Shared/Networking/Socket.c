#include "Socket.h"
#include "ByteConverter.h"
#include "FDIImpl.h"
#include "Logger.h"
#include "Malloc.h"
#include "Opcodes.h"
#include "Packet.h"
#include <assert.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct
{
    uint16_t size;
    uint16_t cmd;
} PacketHdr;
#pragma pack(pop)

static bool _readHeaderHandler(Socket* s);
static bool _readHandler(Socket* s);

static bool _acceptCb(void* socket);
static bool _recvCb(void* socket);
static bool _sendCb(void* socket);

static int _iterateAddrInfo(IPAddress* ip, struct addrinfo* ai, enum SocketMode mode);
static Socket* _initSocket(int fd, IPAddress const* ip, enum SocketMode mode);

Socket* Socket_Create(SocketOpts const* opts)
{
    struct addrinfo hint = { 0 };
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    if (opts->SocketMode == SOCKET_SERVER) // listen socket
        hint.ai_flags = AI_PASSIVE;

    struct addrinfo* ai;
    int res = getaddrinfo(opts->HostName, opts->ServiceOrPort, &hint, &ai);
    if (res != 0)
    {
        LISSANDRA_LOG_ERROR("Socket_Create: getaddrinfo devolvió %i (%s)", res, gai_strerror(res));
        return NULL;
    }

    IPAddress ip;
    int fd = _iterateAddrInfo(&ip, ai, opts->SocketMode);

    freeaddrinfo(ai);

    if (fd < 0)
    {
        LISSANDRA_LOG_ERROR("Socket_Create: imposible conectar! HOST:SERVICIO = %s:%s", opts->HostName,
                            opts->ServiceOrPort);
        return NULL;
    }

    return _initSocket(fd, &ip, opts->SocketMode);
}

bool Socket_IsBlocking(Socket const* s)
{
    return s->Blocking;
}

void Socket_SetBlocking(Socket* s, bool block)
{
    if (block == s->Blocking)
        return;

    int flags = fcntl(s->Handle, F_GETFL, 0);
    if (flags < 0)
    {
        LISSANDRA_LOG_SYSERROR("fcntl");
        return;
    }

    if (block)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    if (fcntl(s->Handle, F_SETFL, flags) < 0)
        LISSANDRA_LOG_SYSERROR("fcntl");

    s->Blocking = block;
}

void Socket_SendPacket(Socket* s, Packet const* packet)
{
    uint16_t opcode = Packet_GetOpcode(packet);
    uint16_t packetSize = Packet_Size(packet);

    PacketHdr header;
    header.cmd = EndianConvert(opcode);
    header.size = EndianConvert(packetSize);

    LISSANDRA_LOG_TRACE("Enviando paquete a %s: %s (opcode: %u, tam: %u)", s->Address.HostIP, opcodeTable[opcode].Name,
                        opcode, packetSize);

    // Socket bloqueante, enviar header+paquete en un solo write
    if (Socket_IsBlocking(s))
    {
        struct iovec iovecs[] =
        {
            { &header,                 sizeof header },
            { Packet_Contents(packet), packetSize    }
        };

        if (writev(s->Handle, iovecs, 2) < 0)
            LISSANDRA_LOG_SYSERROR("writev");
        return;
    }

    size_t payloadSize = sizeof header + header.size;
    if (MessageBuffer_GetRemainingSpace(&s->SendBuffer) < payloadSize)
    {
        MessageBuffer_Normalize(&s->SendBuffer);
        size_t rem = MessageBuffer_GetRemainingSpace(&s->SendBuffer);
        // still not enough?
        if (rem < payloadSize)
        {
            size_t tot = MessageBuffer_GetBufferSize(&s->SendBuffer);
            MessageBuffer_Resize(&s->SendBuffer, tot + payloadSize - rem);
        }
    }

    MessageBuffer_Write(&s->SendBuffer, &header, sizeof header);
    MessageBuffer_Write(&s->SendBuffer, Packet_Contents(packet), packetSize);

    // avisar que tenemos contenido para escribir
    s->Events |= EV_WRITE;
    EventDispatcher_Notify(s);
}

bool Socket_RecvPacket(Socket* s)
{
    // Pre: solo se debe usar con socket bloqueante
    assert(Socket_IsBlocking(s));

    MessageBuffer_Reset(&s->HeaderBuffer);
    ssize_t readLen = recv(s->Handle, MessageBuffer_GetWritePointer(&s->HeaderBuffer), sizeof(PacketHdr), MSG_NOSIGNAL | MSG_WAITALL);
    if (readLen == 0)
    {
        // nos cerraron la conexion
        Socket_Destroy(s);
        return false;
    }

    if (readLen < 0)
    {
        LISSANDRA_LOG_SYSERROR("recv");
        Socket_Destroy(s);
        return false;
    }

    PacketHdr* header = (PacketHdr*) MessageBuffer_GetReadPointer(&s->HeaderBuffer);
    header->size = EndianConvert(header->size);
    header->cmd = EndianConvert(header->cmd);

    if (header->size >= 10240 || header->cmd >= MAX_OPCODE)
    {
        LISSANDRA_LOG_ERROR("_readHeaderHandler(): Cliente %s ha enviado paquete no válido (tam: %hu, opc: %u)",
                            s->Address.HostIP, header->size, header->cmd);
        Socket_Destroy(s);
        return false;
    }

    MessageBuffer_Resize(&s->PacketBuffer, header->size);
    MessageBuffer_Reset(&s->PacketBuffer);

    readLen = recv(s->Handle, MessageBuffer_GetWritePointer(&s->PacketBuffer), header->size, MSG_NOSIGNAL | MSG_WAITALL);
    if (readLen == 0)
    {
        // nos cerraron la conexion
        Socket_Destroy(s);
        return false;
    }

    if (readLen < 0)
    {
        LISSANDRA_LOG_SYSERROR("recv");
        Socket_Destroy(s);
        return false;
    }

    Packet* p = Packet_Create(header->cmd, header->size);
    Packet_Adopt(p, &s->PacketBuffer);

    opcodeTable[header->cmd].HandlerFunction(s, p);

    Packet_Destroy(p);
    return true;
}

void Socket_Destroy(void* elem)
{
    Socket* s = elem;

    MessageBuffer_Destroy(&s->HeaderBuffer);
    MessageBuffer_Destroy(&s->PacketBuffer);
    MessageBuffer_Destroy(&s->SendBuffer);
    MessageBuffer_Destroy(&s->RecvBuffer);
    close(s->Handle);
    Free(s);
}

static bool _readHeaderHandler(Socket* s)
{
    assert(MessageBuffer_GetActiveSize(&s->HeaderBuffer) == sizeof(PacketHdr));

    PacketHdr* header = (PacketHdr*) MessageBuffer_GetReadPointer(&s->HeaderBuffer);
    header->size = EndianConvert(header->size);
    header->cmd = EndianConvert(header->cmd);

    if (header->size >= 10240 || header->cmd >= MAX_OPCODE)
    {
        LISSANDRA_LOG_ERROR("_readHeaderHandler(): Cliente %s ha enviado paquete no válido (tam: %hu, opc: %u)",
                            s->Address.HostIP, header->size, header->cmd);
        return false;
    }

    MessageBuffer_Resize(&s->PacketBuffer, header->size);
    return true;
}

static bool _readDataHandler(Socket* s)
{
    PacketHdr* header = (PacketHdr*) MessageBuffer_GetReadPointer(&s->HeaderBuffer);
    uint16_t opcode = header->cmd;

    Packet* p = Packet_Create(opcode, header->size);
    Packet_Adopt(p, &s->PacketBuffer);

    LISSANDRA_LOG_TRACE("Recibido paquete de %s: %s (opcode: %u, tam: %u)", s->Address.HostIP, opcodeTable[opcode].Name,
                        header->cmd, header->size);

    opcodeTable[opcode].HandlerFunction(s, p);

    Packet_Destroy(p);
    return true;
}

static bool _readHandler(Socket* s)
{
    // try to extract as many whole packets as possible from buffer
    MessageBuffer* mb = &s->RecvBuffer;
    while (MessageBuffer_GetActiveSize(mb) > 0)
    {
        if (MessageBuffer_GetRemainingSpace(&s->HeaderBuffer) > 0)
        {
            size_t readSize = MessageBuffer_GetActiveSize(mb);
            if (MessageBuffer_GetRemainingSpace(&s->HeaderBuffer) < readSize)
                readSize = MessageBuffer_GetRemainingSpace(&s->HeaderBuffer);

            MessageBuffer_Write(&s->HeaderBuffer, MessageBuffer_GetReadPointer(mb), readSize);
            MessageBuffer_ReadCompleted(mb, readSize);

            // no se pudo leer un header entero. Mejor suerte la proxima
            if (MessageBuffer_GetRemainingSpace(&s->HeaderBuffer) > 0)
                break;

            if (!_readHeaderHandler(s))
                return false;
        }

        // We have full read header, now check the data payload
        if (MessageBuffer_GetRemainingSpace(&s->PacketBuffer) > 0)
        {
            // need more data in the payload
            size_t readDataSize = MessageBuffer_GetActiveSize(mb);
            if (MessageBuffer_GetRemainingSpace(&s->PacketBuffer) < readDataSize)
                readDataSize = MessageBuffer_GetRemainingSpace(&s->PacketBuffer);

            MessageBuffer_Write(&s->PacketBuffer, MessageBuffer_GetReadPointer(mb), readDataSize);
            MessageBuffer_ReadCompleted(mb, readDataSize);

            // no se pudo leer un paquete entero. Mejor suerte la proxima
            if (MessageBuffer_GetRemainingSpace(&s->PacketBuffer) > 0)
                break;
        }

        // just received fresh new payload
        bool result = _readDataHandler(s);
        MessageBuffer_Reset(&s->HeaderBuffer);
        if (!result)
            return false;
    }
    return true;
}

static bool _recvCb(void* socket)
{
    Socket* s = socket;

    MessageBuffer_Normalize(&s->RecvBuffer);
    MessageBuffer_EnsureFreeSpace(&s->RecvBuffer);
    ssize_t bytesReceived = recv(s->Handle, MessageBuffer_GetWritePointer(&s->RecvBuffer),
                                 MessageBuffer_GetRemainingSpace(&s->RecvBuffer), MSG_NOSIGNAL);

    if (bytesReceived <= 0)
    {
        // remote end closed connection
        if (!bytesReceived)
            return false;

        // estos errores los ignoramos
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            LISSANDRA_LOG_SYSERROR("recv");
        // sigo existiendo
        return true;
    }

    MessageBuffer_WriteCompleted(&s->RecvBuffer, bytesReceived);
    if (!_readHandler(s))
        return false;
    return true;
}

static bool _sendCb(void* socket)
{
    Socket* s = socket;

    ssize_t bytesSent = send(s->Handle, MessageBuffer_GetReadPointer(&s->SendBuffer),
                             MessageBuffer_GetActiveSize(&s->SendBuffer), MSG_NOSIGNAL);

    LISSANDRA_LOG_DEBUG("_sendCb sent %d bytes", bytesSent);

    if (bytesSent < 0)
    {
        if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)
            LISSANDRA_LOG_SYSERROR("send");
    }
    else if ((size_t) bytesSent == MessageBuffer_GetActiveSize(&s->SendBuffer))
    {
        // escritura terminada, quito el flag de notificación (normalmente los sockets siempre estan listos para escribir)
        s->Events &= (~EV_WRITE);
        EventDispatcher_Notify(s);
    }

    MessageBuffer_ReadCompleted(&s->SendBuffer, bytesSent);
    return true;
}

static bool _acceptCb(void* socket)
{
    Socket* listener = socket;

    struct sockaddr_storage peerAddress;
    socklen_t saddr_len = sizeof peerAddress;
    int fd = accept(listener->Handle, (struct sockaddr*) &peerAddress, &saddr_len);
    if (fd < 0)
    {
        LISSANDRA_LOG_SYSERROR("accept");
        return false;
    }

    IPAddress ip;
    IPAddress_Init(&ip, &peerAddress, saddr_len);
    LISSANDRA_LOG_INFO("Conexión desde %s:%u", ip.HostIP, ip.Port);

    Socket* client = _initSocket(fd, &ip, SOCKET_CLIENT);
    Socket_SetBlocking(client, Socket_IsBlocking(listener));

    EventDispatcher_AddFDI(client);
    return true;
}

static int _iterateAddrInfo(IPAddress* ip, struct addrinfo* ai, enum SocketMode mode)
{
    for (struct addrinfo* i = ai; i != NULL; i = i->ai_next)
    {
        IPAddress_Init(ip, i->ai_addr, i->ai_addrlen);

        char const* address = ip->HostIP;
        uint16_t port = ip->Port;
        unsigned ipv = ip->Version;
        LISSANDRA_LOG_INFO("Socket_Create: Intentado conectar a %s:%u (IPv%u)", address, port, ipv);
        int fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (fd < 0)
        {
            LISSANDRA_LOG_SYSERROR("socket");
            continue;
        }
        else
            LISSANDRA_LOG_INFO("Socket_Create: Creado socket en %s:%u (IPv%u)", address, port, ipv);

        if (mode == SOCKET_SERVER)
        {
            int yes = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0)
            {
                close(fd);
                LISSANDRA_LOG_SYSERROR("setsockopt");
                continue;
            }
        }

        int(*tryFn)(int, struct sockaddr const*, socklen_t) = (mode == SOCKET_CLIENT ? connect : bind);
        if (tryFn(fd, (struct sockaddr const*) &ip->IP, ip->SockAddrLen) < 0)
        {
            close(fd);
            LISSANDRA_LOG_SYSERROR(mode == SOCKET_CLIENT ? "connect" : "bind");
            continue;
        }

        char const* msg = mode == SOCKET_CLIENT ? "Conectado" : "Asociado";
        LISSANDRA_LOG_INFO("Socket_Create: %s a %s:%u! (IPv%u)", msg, address, port, ipv);

        if (mode == SOCKET_SERVER)
        {
            if (listen(fd, BACKLOG) < 0)
            {
                close(fd);
                LISSANDRA_LOG_SYSERROR("listen");
                continue;
            }

            LISSANDRA_LOG_INFO("Socket_Create: Escuchando conexiones entrantes en %s:%u (IPv%u)", address, port, ipv);
        }
        return fd;
    }

    // si llegamos acá es porque fallamos en algo
    return -1;
}

static Socket* _initSocket(int fd, IPAddress const* ip, enum SocketMode mode)
{
    Socket* s = Malloc(sizeof(Socket));
    s->Handle = fd;
    s->Events = EV_READ;
    s->ReadCallback = NULL;
    s->WriteCallback = NULL;
    s->_destroy = Socket_Destroy;

    s->Address = *ip;
    s->Blocking = true;

    MessageBuffer_Init(&s->RecvBuffer, READ_BLOCK_SIZE);
    MessageBuffer_Init(&s->SendBuffer, READ_BLOCK_SIZE);

    MessageBuffer_Init(&s->HeaderBuffer, sizeof(PacketHdr));
    MessageBuffer_Init(&s->PacketBuffer, READ_BLOCK_SIZE);

    switch (mode)
    {
        case SOCKET_SERVER:
            s->ReadCallback = _acceptCb;
            break;
        case SOCKET_CLIENT:
            s->ReadCallback = _recvCb;
            s->WriteCallback = _sendCb;
            break;
        default:
            break;
    }

    return s;
}
