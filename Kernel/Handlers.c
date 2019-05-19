
#include <Opcodes.h>
#include <stddef.h>

OpcodeHandler const opcodeTable[NUM_OPCODES] =
{
    // se conecta un cliente, no deberiamos recibirlo ya que somos clientes nosotros
    { "MSG_HANDSHAKE", NULL },

    // 5 comandos basicos comunes, el kernel los va a enviar al modulo memoria
    // por lo tanto no necesitamos manejarlos.
    { "LQL_SELECT",   NULL },
    { "LQL_INSERT",   NULL },
    { "LQL_CREATE",   NULL },
    { "LQL_DESCRIBE", NULL },
    { "LQL_DROP",     NULL },

    // respuesta de un SELECT
    { "MSG_SELECT", NULL },

    // respuesta del DESCRIBE, almacenar en metadata
    { "MSG_DESCRIBE",        NULL },
    { "MSG_DESCRIBE_GLOBAL", NULL },

    // mensajes a memoria (enviado, no recibido)
    { "LQL_JOURNAL", NULL }
};
