
#ifndef LISSANDRA_LISSANDRA_H
#define LISSANDRA_LISSANDRA_H

typedef struct
{
    char* PUERTO_ESCUCHA;
    char* PUNTO_MONTAJE;
    int RETARDO;
    int TAMANIO_VALUE;
    int TIEMPO_DUMP;
    int TAMANIO_BLOQUES;
    int CANTIDAD_BLOQUES;
} t_config_FS;

extern t_config_FS* confLFS;

#endif //LISSANDRA_LISSANDRA_H
