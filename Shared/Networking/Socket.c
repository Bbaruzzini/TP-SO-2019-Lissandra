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

// tamaño inicial del bufer
static size_t const INIT_ALLOC = 256;

#pragma pack(push, 1)
typedef struct
{
    uint16_t size;
    uint16_t cmd;
} PacketHdr;
#pragma pack(pop)

static bool _acceptCb(void* socket);
static bool _recvCb(void* socket);

static int _iterateAddrInfo(IPAddress* ip, struct addrinfo* ai, enum SocketMode mode);

struct SockInit
{
    int fileDescriptor;
    IPAddress const* ipAddr;
    enum SocketMode mode;

    SocketAcceptFn* acceptFn;
};
static Socket* _initSocket(struct SockInit const* si);

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

    struct SockInit si =
    {
        .fileDescriptor = fd,
        .ipAddr = &ip,
        .mode = opts->SocketMode,

        .acceptFn = opts->SocketOnAcceptClient
    };
    return _initSocket(&si);
}

void Socket_SendPacket(Socket* s, Packet const* packet)
{
    uint16_t opcode = Packet_GetOpcode(packet);
    uint16_t packetSize = Packet_Size(packet);

    PacketHdr header =
    {
        .cmd = EndianConvert(opcode),
        .size = EndianConvert(packetSize)
    };

    LISSANDRA_LOG_TRACE("Enviando paquete a %s: %s (opcode: %u, tam: %u)", s->Address.HostIP, opcodeTable[opcode].Name,
                        opcode, packetSize);

    // Enviar header+paquete en un solo write
    struct iovec iovecs[2] =
    {
        { .iov_base = &header,                 .iov_len = sizeof header },
        { .iov_base = Packet_Contents(packet), .iov_len = packetSize    }
    };

    if (writev(s->Handle, iovecs, 2) < 0)
        LISSANDRA_LOG_SYSERROR("writev");
}

void Socket_Destroy(void* elem)
{
    Socket* s = elem;

    Free(s->PacketBuffer);
    Free(s->HeaderBuffer);

    close(s->Handle);
    Free(s);
}

static bool _recvCb(void* socket)
{
    Socket* s = socket;

    ssize_t readLen = recv(s->Handle, s->HeaderBuffer, sizeof(PacketHdr), MSG_NOSIGNAL | MSG_WAITALL);
    if (readLen == 0)
    {
        // nos cerraron la conexion, limpiar socket
        return false;
    }

    if (readLen < 0)
    {
        // otro error, limpiar socket
        LISSANDRA_LOG_SYSERROR("recv");
        return false;
    }

    PacketHdr* header = (PacketHdr*) s->HeaderBuffer;
    header->size = EndianConvert(header->size);
    header->cmd = EndianConvert(header->cmd);

    if (header->size >= 10240 || header->cmd >= NUM_OPCODES)
    {
        LISSANDRA_LOG_ERROR("_readHeaderHandler(): Cliente %s ha enviado paquete no válido (tam: %hu, opc: %u)",
                            s->Address.HostIP, header->size, header->cmd);
        return false;
    }

    if (header->size > s->PacketBuffSize)
    {
        s->PacketBuffer = Realloc(s->PacketBuffer, header->size);
        s->PacketBuffSize = header->size;
    }

    readLen = recv(s->Handle, s->PacketBuffer, header->size, MSG_NOSIGNAL | MSG_WAITALL);
    if (readLen == 0)
    {
        // nos cerraron la conexion, limpiar socket
        return false;
    }

    if (readLen < 0)
    {
        // otro error
        LISSANDRA_LOG_SYSERROR("recv");
        return false;
    }

    OpcodeHandlerFnType* handler = opcodeTable[header->cmd].HandlerFunction;
    if (!handler)
    {
        LISSANDRA_LOG_ERROR("Socket _recvCb: recibido paquete no soportado! (cmd: %u)", header->cmd);
        return true;
    }

    Packet* p = Packet_Adopt(header->cmd, &s->PacketBuffer, &s->PacketBuffSize);
    handler(s, p);
    Packet_Destroy(p);

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
        return true;
    }

    IPAddress ip;
    IPAddress_Init(&ip, &peerAddress, saddr_len);
    LISSANDRA_LOG_INFO("Conexión desde %s:%u", ip.HostIP, ip.Port);

    struct SockInit si =
    {
        .fileDescriptor = fd,
        .ipAddr = &ip,
        .mode = SOCKET_CLIENT,

        .acceptFn = NULL
    };

    Socket* client = _initSocket(&si);
    if (listener->SockAcceptFn)
        listener->SockAcceptFn(listener, client);

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

static Socket* _initSocket(struct SockInit const* si)
{
    Socket* s = Malloc(sizeof(Socket));
    s->Handle = si->fileDescriptor;
    s->ReadCallback = NULL;
    s->_destroy = Socket_Destroy;

    s->SockAcceptFn = si->acceptFn;

    s->Address = *si->ipAddr;

    s->HeaderBuffer = Malloc(sizeof(PacketHdr));

    s->PacketBuffSize = INIT_ALLOC;
    s->PacketBuffer = Malloc(INIT_ALLOC);

    switch (si->mode)
    {
        case SOCKET_SERVER:
            s->ReadCallback = _acceptCb;
            break;
        case SOCKET_CLIENT:
            s->ReadCallback = _recvCb;
            break;
        default:
            break;
    }

    return s;
}
