
#ifndef Handlers_h__
#define Handlers_h__

typedef struct Socket Socket;
typedef struct Packet Packet;

typedef void HandlerFn (Socket* s, Packet* p);

HandlerFn HandleTestSend;
HandlerFn HandleTestAnswer;

#endif //Handlers_h__
