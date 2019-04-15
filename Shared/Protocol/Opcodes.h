
#ifndef Opcodes_h__
#define Opcodes_h__

typedef struct Packet Packet;
typedef struct Socket Socket;

typedef enum
{
    TEST_SEND,   // mi primer opcode wii, envia un string y un double
    TEST_ANSWER, // responde con otro string

    MAX_OPCODE
} Opcodes;

typedef struct
{
    char const* Name;
    void(*HandlerFunction)(Socket*, Packet*);
} OpcodeHandler;

extern OpcodeHandler const opcodeTable[MAX_OPCODE];

#endif //Opcodes_h__
