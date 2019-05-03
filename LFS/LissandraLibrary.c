//
// Created by Denise on 26/04/2019.
//

#include "LissandraLibrary.h"

reg_lissandra_conexion* cargarArchivoConfiguracion(){

    t_config *archConfig = config_create(PATH_CONFIG);

    if(archConfig== NULL)
    {
        log_error(log_lissandra,"Error al levantar las configuraciones");

        return NULL;
    }

    configLissandra = malloc(sizeof(reg_lissandra_conexion));

    memset(configLissandra, 0, sizeof(reg_lissandra_conexion));

    configLissandra->puerto = config_get_int_value(archConfig, "PUERTO_ESCUCHA");

    configLissandra->punto_montaje = malloc(sizeof(char) * 100); //100 por la cantidad de caracteres de punto_montaje; pueden ser mas o menos

    strcpy(configLissandra->punto_montaje, config_get_string_value(archConfig, "PUNTO_MONTAJE"));

    configLissandra->retardo = config_get_int_value(archConfig, "RETARDO");

    configLissandra->tamanio_value = config_get_int_value(archConfig, "TAMANIO_VALUE");

    configLissandra->tiempo_dump = config_get_int_value(archConfig, "TIEMPO_DUMP");


    log_info(log_lissandra, "Se cargaron correctamente los valores leidos por archivo de configuracion.");

    return configLissandra;

}