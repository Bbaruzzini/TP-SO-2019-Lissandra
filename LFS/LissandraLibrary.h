//
// Created by Denise on 26/04/2019.
//

#ifndef LISSANDRA_LISSANDRALIBRARY_H
#define LISSANDRA_LISSANDRALIBRARY_H

//Linkear bibliotecas!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/log.h>

//ESTRUCTURAS
typedef struct{

    int puerto;
    char* punto_montaje;
    int retardo;
    int tamanio_value;
    int tiempo_dump;

}reg_lissandra_conexion ;

//VARIABLES GLOBALES
reg_lissandra_conexion* configLissandra;
t_log *log_lissandra;

//FUNCIONES
reg_lissandra_conexion* cargarArchivoConfiguracion();


#endif //LISSANDRA_LISSANDRALIBRARY_H
