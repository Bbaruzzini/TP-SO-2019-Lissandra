
#include "Opcodes.h"
#include "Handlers.h"

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    { "TEST_SEND",   HandleTestSend   }, // TEST_SEND
    { "TEST_ANSWER", HandleTestAnswer }  // TEST_ANSWER
};
