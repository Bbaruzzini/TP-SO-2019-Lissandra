
#ifndef Opcodes_h__
#define Opcodes_h__

#include "OpcodeHandler.h"

typedef struct Packet Packet;
typedef struct Socket Socket;

typedef enum
{
    TEST_SEND,   // mi primer opcode wii, envia un string y un double
    TEST_ANSWER, // responde con otro string

    NUM_OPCODES
} Opcodes;

typedef struct
{
    char const* Name;
    OpcodeHandlerFnType* HandlerFunction;
} OpcodeHandler;

extern OpcodeHandler const opcodeTable[NUM_OPCODES];

#endif //Opcodes_h__
