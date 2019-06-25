
#ifndef Gossip_h__
#define Gossip_h__

#include <stdint.h>
#include <vector.h>

typedef struct Socket Socket;

void Gossip_Init(Vector const* seedIPs, Vector const* seedPorts, uint32_t myId, char const* myPort);

void Gossip_AddMemory(uint32_t memId, char const* memIp, char const* memPort);

void Gossip_Do(void);

void Gossip_SendTable(Socket* peer);

void Gossip_Terminate(void);

#endif //Gossip_h__
