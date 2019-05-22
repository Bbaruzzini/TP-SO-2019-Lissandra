
#include "FileSystem.h"
#include "Lissandra.h"
#include "LissandraLibrary.h"
#include "Logger.h"

void iniciarMetadata(){
    pathMetadata = string_new();
    string_append(&pathMetadata, confLFS->PUNTO_MONTAJE);
    string_append(&pathMetadata, "Metadata");
    LISSANDRA_LOG_INFO("Path Metadata %s...", pathMetadata);

    pathBloques=string_new();
    string_append(&pathBloques, confLFS->PUNTO_MONTAJE);
    string_append(&pathBloques, "Bloques");
    LISSANDRA_LOG_INFO("Path Bloques %s...", pathBloques);

    pathTablas = string_new();
    string_append(&pathTablas, confLFS->PUNTO_MONTAJE);
    string_append(&pathTablas, "Tabla");
    LISSANDRA_LOG_INFO("Path Tabla %s...", pathTablas);

    mkdirRecursivo(confLFS->PUNTO_MONTAJE);


    mkdir(pathMetadata, 0777);


    mkdir(pathBloques, 0777);
    mkdir(pathTablas, 0777);

    char *p = string_new();
    string_append(&p, pathMetadata);
    string_append(&p, "/Metadata.bin");

    if(existeArchivo(p)){
        t_config* configAux = config_create(p);
        int bloques = config_get_int_value(configAux, "CANTIDAD_BLOQUES");
        int size = config_get_int_value(configAux, "TAMANIO_BLOQUES");
        if(bloques != confLFS->CANTIDAD_BLOQUES || size != confLFS->TAMANIO_BLOQUES){
            LISSANDRA_LOG_ERROR("Ya Existe un FS en ese punto de montaje con valores distintos");
            exit(1);
        }
        config_destroy(configAux);
    }
    free(p);

    pathMetadataTabla = string_new();
    string_append(&pathMetadataTabla, pathMetadata);
    string_append(&pathMetadataTabla, "/Metadata.bin");

    FILE * metadata = fopen(pathMetadataTabla, "w");
    fprintf(metadata, "TAMANIO_BLOQUES=%d\n", confLFS->TAMANIO_BLOQUES);
    fprintf(metadata, "CANTIDAD_BLOQUES=%d\n", confLFS->CANTIDAD_BLOQUES);
    fprintf(metadata, "MAGIC_NUMBER=LISSANDRA\n");
    fclose(metadata);

    int sizeBitArray = confLFS->CANTIDAD_BLOQUES / 8;
    if((sizeBitArray %8) !=0)
        sizeBitArray++;

    pathMetadataBitarray = string_new();
    string_append(&pathMetadataBitarray, pathMetadata);
    string_append(&pathMetadataBitarray, "/Bitmap.bin");

    if(existeArchivo(pathMetadataBitarray)){
        FILE * bitmap = fopen(pathMetadataBitarray, "rb");

        struct stat stats;
        fstat(fileno(bitmap), &stats);

        char* data = malloc(stats.st_size);
        fread(data, stats.st_size, 1, bitmap);

        fclose(bitmap);

        bitArray = bitarray_create_with_mode(data, stats.st_size, LSB_FIRST);

    }
    else{
        bitArray = bitarray_create_with_mode(string_repeat('\0', sizeBitArray), sizeBitArray, LSB_FIRST);

        FILE * bitmap = fopen(pathMetadataBitarray, "w");
        fwrite(bitArray->bitarray, sizeBitArray, 1, bitmap);
        fclose(bitmap);
    }

    int j;
    FILE* bloque;

    for(j=0; j<confLFS->CANTIDAD_BLOQUES; j++){
        char* pathBloque = string_new();
        string_append(&pathBloque, pathBloques);
        string_append(&pathBloque, "/");
        string_append(&pathBloque, string_itoa(j));
        string_append(&pathBloque, ".bin");

        if(!existeArchivo(pathBloque)){
            bloque = fopen(pathBloque, "w");
            fwrite(string_repeat('\0', confLFS->TAMANIO_BLOQUES), confLFS->TAMANIO_BLOQUES, 1, bloque);
            fclose(bloque);
        }
        free(pathBloque);
    }

    LISSANDRA_LOG_DEBUG("Se finalizo la creacion de Metadata");



}

/*
char* generarPathTabla(char* path){

    LISSANDRA_LOG_INFO("Se genererara el Path Absoluto del archivo");
    char* pathAbs = string_new();
    string_append(&pathAbs, confLFS->PUNTO_MONTAJE);
    string_append(&pathAbs, "Archivos");

    if(!string_starts_with(path, "/")) string_append(&pathAbs, "/");

    string_append(&pathAbs, path);


    return pathAbs;


}
*/

void mkdirRecursivo(char* path){

    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",path);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    mkdir(tmp, 0777);
}

bool existeArchivo(char* path){

    FILE * archi = fopen(path, "r");
    if(archi != NULL){
        fclose(archi);
        return true;
    }
    else{
        return false;
    }
}

/*
void escribirValorBitarray(bool valor, int pos){
    if(valor)
        bitarray_set_bit(bitArray, pos);
    else
        bitarray_clean_bit(bitArray, pos);

    FILE *bitmap = fopen(pathMetadataBitarray, "w");

    fwrite(bitArray->bitarray, bitArray->size, 1, bitmap);
    fclose(bitmap);
    return;
}
*/

/*
char* generarPathBloque(int num_bloque){
    char* path_bloque = string_new();
    string_append(&path_bloque, confLFS->PUNTO_MONTAJE);
    string_append(&path_bloque, "Bloques/");
    string_append(&path_bloque, string_itoa(num_bloque));
    string_append(&path_bloque, ".bin");

    return path_bloque;
}
*/
/*
int cantidadBloques(char** bloques){
    int j = 0;

    while(bloques[j] != NULL)
        j++;

    return j;
}

*/
