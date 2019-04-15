
#include "Opcodes.h"
#include "Handlers.h"

OpcodeHandler const opcodeTable[MAX_OPCODE] =
{
    { "TEST_SEND",   HandleTestSend   }, // TEST_SEND
    { "TEST_ANSWER", HandleTestAnswer }  // TEST_ANSWER
};
