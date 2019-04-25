
#ifndef Handlers_h__
#define Handlers_h__

typedef struct Socket Socket;
typedef struct Packet Packet;

typedef void HandlerFn (Socket* s, Packet* p);

#endif //Handlers_h__
