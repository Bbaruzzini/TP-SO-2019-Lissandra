
#include "MainMemory.h"
#include "Page.h"
#include <Config.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdint.h>

// la memoria es un arreglo de páginas contiguas
static Page* Memory = NULL;

void Memory_Init(uint32_t maxValueLength)
{
    pthread_rwlock_rdlock(&sConfigLock);
    // esto es en bytes o en registros?
    size_t allocSize = config_get_long_value(sConfig, "TAM_MEM");
    pthread_rwlock_unlock(&sConfigLock);

    // asumo bytes luego vemos como arreglarnos con posibles imparidades.
    // todo: consultar
    Memory = Malloc(allocSize);

    // el valor se guarda contiguo junto a los otros datos de longitud fija
    // si es menor al maximo se halla null terminated de lo contrario no
    size_t const pageSize = sizeof(Page) + maxValueLength;
    size_t const numPages = allocSize / pageSize;

    LISSANDRA_LOG_INFO("Memoria inicializada. Tamaño: %d bytes (%d páginas). Tamaño de página: %d", allocSize, numPages, pageSize);
}

bool Memory_IsFull(void)
{
    //return SegmentTable_IsFull();
    return false;
}

void Memory_Terminate(void)
{
    Free(Memory);
}
