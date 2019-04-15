
#include "Handlers.h"
#include "Logger.h"
#include "Opcodes.h"
#include "Packet.h"
#include "Socket.h"

void HandleTestSend(Socket* s, Packet* p)
{
    char* string;
    double d;

    Packet_Read(p, &string);
    Packet_Read(p, &d);

    LISSANDRA_LOG_TRACE("TEST_SEND: %s", string);
    LISSANDRA_LOG_TRACE("TEST_SEND: %.10f", d);
    Free(string);

    Packet* ans = Packet_Create(TEST_ANSWER, 0);
    Packet_Append(ans, "Hola de nuevo!");
    Socket_SendPacket(s, ans);
    Packet_Destroy(ans);
}

void HandleTestAnswer(Socket* s, Packet* p)
{
    (void) s;

    char* string;
    Packet_Read(p, &string);
    LISSANDRA_LOG_TRACE("TEST_ANSWER: %s", string);
    Free(string);
}
