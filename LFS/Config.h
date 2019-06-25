
#ifndef Config_h__
#define Config_h__

#include <Defines.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    char PUERTO_ESCUCHA[PORT_STRLEN];
    char PUNTO_MONTAJE[PATH_MAX];
    uint32_t RETARDO;
    uint32_t TAMANIO_VALUE;
    uint32_t TIEMPO_DUMP;
    size_t TAMANIO_BLOQUES;
    size_t CANTIDAD_BLOQUES;
} t_config_FS;

extern t_config_FS confLFS;

#endif //Config_h__
