
#ifndef Page_h__
#define Page_h__

#include <stdint.h>
#include <time.h>

// Una pagina es un registro
// todo: empaquetar esta estructura?
typedef struct
{
    uint16_t Key;
    time_t Timestamp;
    char Value[]; // tamaño maximo seteado en tiempo de ejecucion!
} Page;

#endif //Page_h__
