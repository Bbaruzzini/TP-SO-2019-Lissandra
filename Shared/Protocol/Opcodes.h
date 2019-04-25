
#ifndef Opcodes_h__
#define Opcodes_h__

#include "OpcodeHandler.h"

typedef struct Packet Packet;
typedef struct Socket Socket;

typedef enum
{
    // Mensajes que entienden los 3 modulos
    // requests
    LQL_SELECT,  /* char*: nombre tabla
                  * uint16: key
                  *
                  * Responde: MSG_SELECT
                  */

    LQL_INSERT,  /* char*: nombre tabla
                  * uint16: key
                  * char*: value
                  * uint32 OPCIONAL: timestamp
                  */

    LQL_CREATE,  /* char*: nombre tabla
                  * uint8: tipo consistencia (ver Kernel/Criteria.h)
                  * uint16: numero particiones
                  * uint32: tiempo entre compactaciones, en milisegundos
                  */

    LQL_DESCRIBE, /* char* OPCIONAL: nombre tabla
                   *
                   * Responde: MSG_DESCRIBE o MSG_DESCRIBE_GLOBAL
                   */

    LQL_DROP,     /* char*: nombre tabla */

    // Mensajes que entienden los 3 modulos
    // Respuestas a queries
    MSG_SELECT,  /* char*: nombre tabla
                  * uint16: key
                  * char*: value
                  * uint32: timestamp
                  */

    MSG_DESCRIBE, /* char*: nombre tabla
                   * uint8: tipo de consistencia (ver Kernel/Criteria.h)
                   * uint16: numero de particiones
                   * uint32: tiempo entre compactaciones, en milisegundos
                   */

    MSG_DESCRIBE_GLOBAL, /* uint32: cantidad de tablas
                          * cada una se deserializa igual que MSG_DESCRIBE
                          */

    NUM_OPCODES
} Opcodes;

typedef struct
{
    char const* Name;
    OpcodeHandlerFnType* HandlerFunction;
} OpcodeHandler;

extern OpcodeHandler const opcodeTable[NUM_OPCODES];

#endif //Opcodes_h__
