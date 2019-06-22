
#include "FileSystem.h"
#include "Lissandra.h"
#include "LissandraLibrary.h"
#include <libcommons/config.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdio.h>
#include <sys/stat.h>

char pathMetadataBitarray[PATH_MAX] = { 0 };

t_bitarray* bitArray = NULL;

void iniciarFileSystem(void)
{
    char pathMetadata[PATH_MAX];
    snprintf(pathMetadata, PATH_MAX, "%sMetadata", confLFS->PUNTO_MONTAJE);
    LISSANDRA_LOG_INFO("Path Metadata %s...", pathMetadata);

    char pathBloques[PATH_MAX];
    snprintf(pathBloques, PATH_MAX, "%sBloques", confLFS->PUNTO_MONTAJE);
    LISSANDRA_LOG_INFO("Path Bloques %s...", pathBloques);

    char pathTablas[PATH_MAX];
    snprintf(pathTablas, PATH_MAX, "%sTables", confLFS->PUNTO_MONTAJE);
    LISSANDRA_LOG_INFO("Path Tables %s...", pathTablas);

    mkdirRecursivo(confLFS->PUNTO_MONTAJE);

    mkdir(pathMetadata, 0700);
    mkdir(pathBloques, 0700);
    mkdir(pathTablas, 0700);

    char metadataFile[PATH_MAX];
    snprintf(metadataFile, PATH_MAX, "%s/Metadata.bin", pathMetadata);

    if (existeArchivo(metadataFile))
    {
        t_config* configAux = config_create(metadataFile);
        int bloques = config_get_int_value(configAux, "CANTIDAD_BLOQUES");
        int size = config_get_int_value(configAux, "TAMANIO_BLOQUES");
        LISSANDRA_LOG_INFO("Ya Existe un FS en ese punto de montaje con %d bloques de %d bytes de tamanio", bloques, size);
        if (bloques != confLFS->CANTIDAD_BLOQUES || size != confLFS->TAMANIO_BLOQUES)
        {
            confLFS->CANTIDAD_BLOQUES = bloques;
            confLFS->TAMANIO_BLOQUES = size;
        }
        config_destroy(configAux);
    }
    else
    {
        FILE* metadata = fopen(metadataFile, "w");
        fprintf(metadata, "TAMANIO_BLOQUES=%d\n", confLFS->TAMANIO_BLOQUES);
        fprintf(metadata, "CANTIDAD_BLOQUES=%d\n", confLFS->CANTIDAD_BLOQUES);
        fprintf(metadata, "MAGIC_NUMBER=LISSANDRA\n");
        fclose(metadata);
    }

    size_t sizeBitArray;
    char* data;

    snprintf(pathMetadataBitarray, PATH_MAX, "%s/Bitmap.bin", pathMetadata);
    if (existeArchivo(pathMetadataBitarray))
    {
        FILE* bitmap = fopen(pathMetadataBitarray, "rb");

        struct stat stats;
        fstat(fileno(bitmap), &stats);

        sizeBitArray = stats.st_size;
        data = Malloc(stats.st_size);
        fread(data, stats.st_size, 1, bitmap);

        fclose(bitmap);
    }
    else
    {
        sizeBitArray = confLFS->CANTIDAD_BLOQUES / 8;
        if (sizeBitArray % 8)
            ++sizeBitArray;

        data = Calloc(sizeBitArray, 1);

        FILE* bitmap = fopen(pathMetadataBitarray, "w");
        fwrite(data, sizeBitArray, 1, bitmap);
        fclose(bitmap);
    }

    bitArray = bitarray_create_with_mode(data, sizeBitArray, LSB_FIRST);

    if (dirIsEmpty(pathBloques))
    {
        for (int j = 0; j < confLFS->CANTIDAD_BLOQUES; ++j)
        {
            char pathBloque[PATH_MAX];
            snprintf(pathBloque, PATH_MAX, "%s/%d.bin", pathBloques, j);

            char* emptyArray = Calloc(confLFS->TAMANIO_BLOQUES, 1);
            if (!existeArchivo(pathBloque))
            {
                FILE* bloque = fopen(pathBloque, "w");
                fwrite(emptyArray, confLFS->TAMANIO_BLOQUES, 1, bloque);
                fclose(bloque);
            }
            Free(emptyArray);
        }
    }
    else
    {
        /// todo?
    }

    LISSANDRA_LOG_TRACE("Se finalizo la creacion del File System");
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
