
#ifndef LISSANDRA_COMPACTADOR_H
#define LISSANDRA_COMPACTADOR_H

#include <Logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libcommons/string.h>
#include "Memtable.h"
#include "Lissandra.c"
#include "LissandraLibrary.h"
#include <libcommons/config.h>


enum resultados_operaciones { CONTINUAR, FINALIZAR };

//hilos
pthread_mutex_t hilos_compactadores_mutex = PTHREAD_MUTEX_INITIALIZER;
t_dictionary *hilos_compactadores;

typedef int (*operacion_t)(t_registro);

//funciones para manejo Compactacion

int inicializarCompactador();
void destruirHiloCompactador(void* hilo);
int destruirCompactador();
void liberarArrayRegistros(t_registro* array, int cantidad);
int agregarRegistroAParticion(t_registro** registros, int* cantidadActual, int* maximo, t_registro unNuevoRegistro);
int getTamanioValue();
int getTamanioDelStringRegistro();
int dePathABloque(char* numeroDeBloque, char* buffer, int tamanioBuffer);
int leerHastaSiguienteToken(FILE* archivo, char* resultado, char token);
int traduccionDelRegistro(char* stringRegistroOLoQueSea, t_registro* registro);
int iterarBloque(char* pathBloque, char* registroATruncar, operacion_t operacion);
int iterarArchivoDeDatos(char* path, operacion_t operacion);
int leerArchivoDeDatos(char* path, t_registro** resultado);
int existeTabla(char* nombreTabla);
int iterarDirectorioTabla(char* tabla, int (funcion)(const char*, const struct stat*, int));
void convertirTodosTmp_a_Tmpc(char* tabla);
int leerParticion(char* nombreTabla, int numeroParticion, t_registro** registros);
void compactarParticion(char* nombreTabla, int nroParticion, int totalParticiones, t_registro* datosTmpc, int cantidad);
int compactar(char *nombreTabla);
void *threadCompactacion(void* data);
int instanciarHiloCompactador(char* tabla, int t_compactaciones);
int terminarHiloCompactacion(char* tabla);
void compactarTodasLasTablas();

int cargarElRegistro(t_registro registro);
void obtenerPathTabla(char* nombreTabla, char* path);



//funciones manejo archivo
FILE *abrirArchivoParaLectura(char* path);


#endif //LISSANDRA_COMPACTADOR_H
