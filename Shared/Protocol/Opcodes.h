
#ifndef Opcodes_h__
#define Opcodes_h__

#include "OpcodeHandler.h"

typedef struct Packet Packet;
typedef struct Socket Socket;

enum
{
    KERNEL = 1,
    MEMORIA = 2,
    LFS = 3
};

typedef enum
{
    // Mensajes que entienden los 3 modulos
    MSG_HANDSHAKE, /* mensaje enviado al conectar
                    *
                    * uint8_t: module id (ver enum)
                    *
                    * Responde: MSG_HANDSHAKE_RESPUESTA (LFS->Memoria)
                    *           MSG_MEMORY_ID (Memoria->Kernel)
                    */

    // requests
    LQL_SELECT,  /* char*: nombre tabla
                  * uint16: key
                  *
                  * Responde: MSG_SELECT
                  */

    LQL_INSERT,  /* char*: nombre tabla
                  * uint16: key
                  * char*: value
                  * uint64: timestamp (de K->M no posee)
                  */

    LQL_CREATE,  /* char*: nombre tabla
                  * uint8: tipo consistencia (ver Consistency.h)
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
    MSG_SELECT,  /* char*: value
                  */

    MSG_DESCRIBE, /* char*: nombre tabla
                   * uint8: tipo de consistencia (ver Consistency.h)
                   * uint16: numero de particiones
                   * uint32: tiempo entre compactaciones, en milisegundos
                   */

    MSG_DESCRIBE_GLOBAL, /* uint32: cantidad de tablas
                          * cada una se deserializa igual que MSG_DESCRIBE
                          */

    // Mensajes a memoria
    LQL_JOURNAL,        /* nada */

    MSG_HANDSHAKE_RESPUESTA, /* uint32_t: tamanioValue
                              * char*: puntoMontaje
                              */

    MSG_CREATE_RESPUESTA,   /*uint8: EXIT_SUCCESS o EXIT_FAILURE
                            */

    MSG_DROP_RESPUESTA, /*uint8: EXIT_SUCCESS o EXIT_FAILURE
                        */

    MSG_INSERT_RESPUESTA,   /*uint8: EXIT_SUCCESS o EXIT_FAILURE
                            */

    MSG_MEMORY_ID,           /* uint32_t: memoryId
                              */


    // erores
    MSG_ERR_NOT_FOUND, // key no encontrado

    MSG_ERR_MEM_FULL,  /* memoria está full. Si es SELECT contiene:
                        *
                        * char*: value
                        */

    MSG_ERR_TABLE_NOT_EXISTS, // tabla no existe (ejemplo DROP, DESCRIBE)

    NUM_OPCODES
} Opcodes;

extern OpcodeHandler const opcodeTable[NUM_OPCODES];

#endif //Opcodes_h__
