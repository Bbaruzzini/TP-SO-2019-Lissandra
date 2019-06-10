
#ifndef Frame_h__
#define Frame_h__

#include <stdint.h>
#include <time.h>

// Una frame es la página en memoria (registro)
// todo: empaquetar esta estructura?
typedef struct
{
    uint32_t Timestamp;
    uint16_t Key;
    char Value[]; // tamaño maximo seteado en tiempo de ejecucion!
} Frame;

typedef struct
{
    char const* TableName;
    uint32_t Timestamp;
    uint16_t Key;
    char const* Value;
} DirtyFrame;

#endif //Frame_h__
