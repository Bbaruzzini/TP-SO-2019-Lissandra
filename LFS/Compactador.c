//
// "esto es una prueba, dejame probar CLion "
//

#include "Compactador.h"
 /*
  *
//Intento de uso de hilos (fail), somebody help

int inicializarCompactador() {
    if(pthread_mutex_lock(&hilos_compactadores_mutex) != 0) {
        return -1;
    }
    hilos_compactadores = dictionary_create();
    pthread_mutex_unlock(&hilos_compactadores_mutex);
    return hilos_compactadores == NULL ? -1 : 0;
}

void destruirHiloCompactador(void* hilo) {

    //TODO

    free(hilo);
}

int destruirCompactador() {
    if(pthread_mutex_lock(&hilos_compactadores_mutex) != 0) {
        return -1;
    }
    dictionary_destroy_and_destroy_elements(hilos_compactadores, &destruirHiloCompactador);
    pthread_mutex_unlock(&hilos_compactadores_mutex);
    return 0;
}

void liberarArrayRegistros(t_registro* array, int cantidad) {
    for(int i = 0; i < cantidad; i++) {
        free(array[i].value);
        array[i].value = NULL;
    }
    free(array);
}

int agregarRegistroAParticion(t_registro** registros, int* cantidadActual, int* maximo, t_registro unNuevoRegistro) {
    for(int i = 0; i < *cantidadActual; i++) {
        if((*registros)[i].key == unNuevoRegistro.key) {
            if((*registros)[i].timestamp < unNuevoRegistro.timestamp) {
                free((*registros)[i].value);
                (*registros)[i] = unNuevoRegistro;
            } else {
                free(unNuevoRegistro.value);
            }
            return 0;
        }
    }
    if(*cantidadActual >= *maximo) {
        *maximo = *maximo == 0 ? 10 : *maximo * 2;
        t_registro *nuevosRegistros = realloc(*registros, *maximo * sizeof(t_registro));
        if(nuevosRegistros == NULL) {
            return -1;
        }
        *registros = nuevosRegistros;
    }
    (*registros)[*cantidadActual] = unNuevoRegistro;
    (*cantidadActual)++;
    return 0;
}


obtenerPathParticion(int numeroParticion, char tabla, pathDeLaParticion);

int getTamanioValue() {
    // Wrapper para llamar a get_tamanio_value solo una vez
    // y no constantemente cada vez que se itera, así
    // evitamos el overhead de bloquear el semáforo
    // de lfs-config
    static int tamanio = -1;
    if (tamanio == -1) {
        tamanio = confLFS.TAMANIO_VALUE;
    }
    return tamanio;
}


int getTamanioDelStringRegistro() {
    // El tamanio maximo del string de un registro es:
    // El tamanio del value +
    // 20 bytes del timestamp +
    // 5 bytes de la key +
    // 2 bytes de los ';' +
    // 1 byte del '\0'

    // Por la misma razón que en getTamanioValue(),
    // almacenamos el valor
    static int tamanio = -1;
    if (tamanio == -1) {
        tamanio = getTamanioValue() + __size_t(t_registro.timestamp) + 5 + 2 + 1;
    }
    return tamanio;
}

int dePathABloque(char* numeroDeBloque, char* buffer, int tamanioBuffer) { //obtiene el path del bloque
    if (tamanioBuffer < PATH_MAX) {
        return -1;
    }
    sprintf(buffer, "%sBloques/%s.bin", confLFS.PUNTO_MONTAJE , numeroDeBloque);
    return 0;
}

int leerHastaSiguienteToken(FILE* archivo, char* resultado, char token) {

    // Recibe un archivo, un string 'resultado' y un caracter 'token'.
    // Le el archivo hasta encontrar el token y graba lo que lee en
    // 'resultado' sin incluir el token.
    // Retorna:
    //      * -1 en caso de error
    //      * 0 si se llegó al EOF
    //      * 1 si salió todo bien

    char caracterLeido, buffer_cat[2] = { 0 };
    if (fread(&caracterLeido, 1, 1, archivo) != 1) {
        // Retornamos 0 si termino el archivo, -1 ante un error
        return feof(archivo) ? 0 : -1;
    }
    while (caracterLeido != token) {
        buffer_cat[0] = caracterLeido;
        strcat(resultado, buffer_cat);
        if (fread(&caracterLeido, 1, 1, archivo) != 1) {
            // Retornamos 0 si termino el archivo, -1 ante un error
            return feof(archivo) ? 0 : -1;
        }
    }
    return 1;
}

int traduccionDelRegistro(char* stringRegistroOLoQueSea, t_registro* registro) {

    // Recibe el string de un registro con el formato "TIMESTAMP;KEY;VALUE"
    // y un puntero a un registro_t.
    // Parsea el string y carga en el registro los valores.

    char buffer[getTamanioDelStringRegistro()];
    int indice_string_registro = 0, indice_buffer = 0;

    void siguienteSubstring() {
        memset(buffer, 0, getTamanioDelStringRegistro());
        while (stringRegistroOLoQueSea[indice_string_registro] != ';'
               && stringRegistroOLoQueSea[indice_string_registro] != '\0') {
            buffer[indice_buffer++] = stringRegistroOLoQueSea[indice_string_registro++];
        }
        indice_string_registro++;
        indice_buffer = 0;
    }

    siguienteSubstring();
    registro->timestamp = (time_t) strtoumax(buffer, NULL, 10);
    siguienteSubstring();
    registro->key = (uint16_t) strtoumax(buffer, NULL, 10);
    siguienteSubstring();
    registro->value = malloc(strlen(buffer) + 1);
    if (registro->value == NULL) {
        return -1;
    }
    strcpy(registro->value, buffer);
    return 0;
}


int iterarBloque(char* pathBloque, char* registroATruncar, operacion_t operacion) {

    // Recibe la dirección de un bloque en 'pathBloque' y por cada
    // registro del bloque ejecuta 'operación' con el registro_t
    // correspondiente como parámetro. En caso de que la operación
    // retorne algo distínto de CONTINUAR, se cortará la iteración.

    // Por medio de 'registro_truncado' recibe el posible registro
    // que haya quedado incompleto por una iteración a un bloque
    // anterior para así completarlo. A su vez, si hay un registro
    // cortado al final de este bloque, se almacena en 'registro_truncado'

    FILE *archivoBloque = abrirArchivoParaLectura(pathBloque);
    char stringRegistro[getTamanioDelStringRegistro()];
    int resultado;
    t_registro registro;
    // Si había un registro truncado, lo restauramos
    strcpy(stringRegistro, registroATruncar);
    memset(registroATruncar, 0, getTamanioDelStringRegistro());
    if (archivoBloque == NULL) {
        return errno == ENOENT ? FINALIZAR : -1;
    }

    while ((resultado = leerHastaSiguienteToken(archivoBloque, stringRegistro, '\n')) == 1) {
        if (traduccionDelRegistro(stringRegistro, &registro) < 0) {
            resultado = -1;
            break;
        }
        if ((resultado = operacion(registro)) != CONTINUAR) {
            break;
        }
        memset(stringRegistro, 0, getTamanioDelStringRegistro());
    }

    fclose(archivoBloque);

    if (resultado == 0) {
        // EOF, manejamos el posible registro truncado
        strcpy(registroATruncar, stringRegistro);
        return CONTINUAR;
    }
    return resultado;
}

int iterarArchivoDeDatos(char* path, operacion_t operacion) {

    // Itera todos los bloques asociados al archivo de datos
    // (partición o temporal) 'path' usando la función
    // 'iterarBloque' en cada uno y ejecutando 'operacion'
    // por cada registro. También maneja los registros truncados
    // entre bloques.

    FILE *archivo = abrirArchivoParaLectura(path);
    if (archivo == NULL) {
        return -1;
    }
    t_config *archivoDatos = lfsConfigCrearDesdeFile(path, archivo);
    if (archivoDatos == NULL) {
        fclose(archivo);
        return -1;
    }

    char registroTruncado[getTamanioDelStringRegistro()];
    char pathBloque[PATH_MAX] = { 0 };
    int resultado;

    memset(registroTruncado, 0, getTamanioDelStringRegistro());

    char **bloques = config_get_array_value(archivoDatos, "BLOCKS");
    if (bloques == NULL) {
        config_destroy(archivoDatos);
        fclose(archivo);
        return -1;
    }

    for (char **numeroBloque = bloques; *numeroBloque != NULL; numeroBloque++) {
        dePathABloque(*numeroBloque, pathBloque, PATH_MAX);
        resultado = iterarBloque(pathBloque, registroTruncado, operacion);
        if (resultado != CONTINUAR) {
            break;
        }
    }

    fclose(archivo);

    for (char **bloqueNum = bloques; *bloqueNum != NULL; bloqueNum++) {
        free(*bloqueNum);
    }
    free(bloques);
    config_destroy(archivoDatos);
    return resultado < 0 ? -1 : 0;
}


int leerArchivoDeDatos(char* path, t_registro** resultado) {

    // Recibe un archivo de datos 'path' y un doble puntero
    // a registro. Aloca memoria al puntero y graba en él
    // todos los registros del archivo de datos.

    // Arrancamos con lugar para 10 registros
    int tamanioActual = 10 * sizeof(t_registro), registrosYaCargados = 0;
    bool existioError = false;

    *resultado = malloc(tamanioActual);
    if (*resultado == NULL) {
        return -1;
    }

    int cargarElRegistro(t_registro registro){
        t_registro *puntero_realocado;
        if (registrosYaCargados >= tamanioActual) {
            tamanioActual *= 2;
            puntero_realocado = realloc(*resultado, tamanioActual);
            if (puntero_realocado == NULL) {
                existioError = true;
                return FINALIZAR;
            }
            *resultado = puntero_realocado;
        }
        (*resultado)[registrosYaCargados++] = registro;
        return CONTINUAR;
    }

    if (iterarArchivoDeDatos(path, &cargarElRegistro) < 0 || existioError) {
        // Error, liberar resultado
        for (int i = 0; i < registrosYaCargados; i++) {
            free((*resultado)[i].value);
        }
        free(*resultado);
        return -1;
    }

    return registrosYaCargados;
}


int existeTabla(char* nombreTabla) {
    char path_tabla[PATH_MAX];
    struct stat buffer;

    obtenerPathTabla(nombreTabla, path_tabla);

    if (stat(path_tabla, &buffer) < 0) {
        if (errno == ENOENT) {
            return 1;
        }
        return -1;
    }
    return 0;
}


int iterarDirectorioTabla(char* tabla, int (funcion)(const char*, const struct stat*, int)) {

    char pathTabla[PATH_MAX] = { 0 };
    obtenerPathTabla(tabla, pathTabla);
    return ftw(pathTabla, funcion, 10);

}

void convertirTodosTmp_a_Tmpc(char* tabla) {

    if (existeTabla(tabla) != 0) {
        //log no existe tabla o hubo error
        return;
    }

    int renombrarArchivoATmpc(const char* path, const struct stat* stat, int flag) {
        if (string_ends_with((char*) path, ".tmp")
            || string_ends_with((char*) path, ".tmp/")) {

            char ppathNuevoTemp[PATH_MAX];
            strcpy(ppathNuevoTemp, path);
            strcat(ppathNuevoTemp, "c");
            rename(path, ppathNuevoTemp);
        }
        return 0;
    }
    iterarDirectorioTabla(tabla, &renombrarArchivoATmpc);
}


int leerParticion(char* nombreTabla, int numeroParticion, t_registro** registros) {
    char pathParticion[PATH_MAX];
    obtenerPathParticion(numeroParticion, nombreTabla, pathParticion);
    return leerArchivoDeDatos(pathParticion, registros);


void compactarParticion(char* nombreTabla, int nroParticion, int totalParticiones, t_registro* datosTmpc, int cantidad) {

    t_registro *datosParticion;
    int ret;
    int cantidadDeRegistrosEnParticion = leerParticion(nombreTabla, nroParticion, &datosParticion);
    int tamanioBufferParticion = cantidadDeRegistrosEnParticion;
    if(cantidadDeRegistrosEnParticion < 0) {
        // log
        return;
    }
    for(int i = 0; i < cantidad; i++) {
        if(datosTmpc[i].key % totalParticiones == nroParticion) {
            ret = agregarRegistroAParticion(&datosParticion, &cantidadDeRegistrosEnParticion, &tamanioBufferParticion, datosTmpc[i]);
            if(ret < 0) {
                liberarArrayRegistros(datosParticion, cantidadDeRegistrosEnParticion);
                // log
                return;
            }
        }
    }
    if(pisarParticion(nombreTabla, nroParticion, datosParticion, cantidadDeRegistrosEnParticion) < 0) {
        // log
    }
    liberarArrayRegistros(datosParticion, cantidadDeRegistrosEnParticion);
}

int compactar(char *nombreTabla) {

    static int metadataCargada = 0;
    static t_describe tDescribe; //los datos del struct en LissandraLibrary.h

    if(!metadataCargada) {
        if(obtenerMetadataTabla(nombreTabla, &tDescribe) < 0) {
            // log
            return -1;
        }
        metadataCargada = 1;
    }

    t_registro *datosTmpc;
    convertirTodosTmp_a_Tmpc(nombreTabla);
    int cantidadRegistrosTmpc = obtenerDatosDeTmpcs(nombreTabla, &datosTmpc);
    if(cantidadRegistrosTmpc < 0) {
        // log
        return -1;
    }

    for(int particion = 0; particion < tDescribe.n_particiones; particion++) {
        compactarParticion(nombreTabla, particion, tDescribe.n_particiones, datosTmpc, cantidadRegistrosTmpc);
    }

    free(datosTmpc);
    borrarTodosLosTmpc(nombreTabla);
    return -1;
}

//ESTO ES UN INTENTO DE HILOS TIRANDO A FRACASO, LO DEJO POR SI ALGUNO SE INSPIRA Y LO ARREGLA :B
void *threadCompactacion(void* data) {
    lissandra_thread_t *l_thread = (lissandra_thread_t*) data;
    char *nombreTabla = (char*) l_thread->entrada;
    compactar(nombreTabla);
    return NULL;
}

int instanciarHiloCompactador(char* tabla, int t_compactaciones) {
    lissandra_thread_periodic_t *lp_thread;
    char *entradaThread;
    if(pthread_mutex_lock(&hilos_compactadores_mutex) != 0) {
        return -1;
    }
    if(dictionary_has_key(hilos_compactadores, tabla)) {
        pthread_mutex_unlock(&hilos_compactadores_mutex);
        return -1;
    }
    entradaThread = strdup(tabla);
    if(entradaThread == NULL) {
        pthread_mutex_unlock(&hilos_compactadores_mutex);
        return -1;
    }
    lp_thread = malloc(sizeof(lissandra_thread_periodic_t));
    if(lp_thread == NULL) {
        pthread_mutex_unlock(&hilos_compactadores_mutex);
        free(entradaThread);
        return -1;
    }
    if(l_thread_periodic_create_fixed(lp_thread, &thread_compactacion, t_compactaciones, entradaThread) < 0) {
        pthread_mutex_unlock(&hilos_compactadores_mutex);
        free(lp_thread);
        free(entradaThread);
        return -1;
    }
    dictionary_put(hilos_compactadores, tabla, lp_thread);
    pthread_mutex_unlock(&hilos_compactadores_mutex);
    return 0;
}

int terminarHiloCompactacion(char* tabla) {
    if(pthread_mutex_lock(&hilos_compactadores_mutex) != 0) {
        return -1;
    }
    if(!dictionary_has_key(hilos_compactadores, tabla)) {
        pthread_mutex_unlock(&hilos_compactadores_mutex);
        return -1;
    }
    void *hiloCompactador = dictionary_remove(hilos_compactadores, tabla);
    destruirHiloCompactador(hiloCompactador);
    pthread_mutex_unlock(&hilos_compactadores_mutex);
    return 0;
}

void compactarTodasLasTablas() {

    int resultado = 0;

    void compactar(char *tabla, void *valor) {
        int resCompactacion = compactar(tabla);
        resultado = resultado || resCompactacion;
    }

    pthread_mutex_lock(&hilos_compactadores_mutex);
    dictionary_iterator(hilos_compactadores, &_compactar);
    pthread_mutex_unlock(&hilos_compactadores_mutex);
}
*/