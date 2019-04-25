
#include "Opcodes.h"
#include "Kernel/Handlers.h"

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    { "LQL_SELECT",   0 },
    { "LQL_INSERT",   0 },
    { "LQL_CREATE",   0 },
    { "LQL_DESCRIBE", HandleDescribeOpcode }
};
