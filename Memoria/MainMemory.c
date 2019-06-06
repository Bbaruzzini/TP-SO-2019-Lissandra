
#include "MainMemory.h"
#include "Page.h"
#include "PageTable.h"
#include "SegmentTable.h"
#include <Config.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdint.h>

// la memoria es un arreglo de páginas contiguas
static Page* Memory = NULL;
static uint32_t MaxValueLength = 0;

void Memory_Initialize(uint32_t maxValueLength, char const* mountPoint)
{
    pthread_rwlock_rdlock(&sConfigLock);
    size_t allocSize = config_get_long_value(sConfig, "TAM_MEM");
    pthread_rwlock_unlock(&sConfigLock);

    // malloc de n bytes contiguos
    Memory = Malloc(allocSize);
    MaxValueLength = maxValueLength;

    // el valor se guarda contiguo junto a los otros datos de longitud fija
    // si es menor al maximo se halla null terminated de lo contrario no
    size_t const pageSize = sizeof(Page) + maxValueLength;
    size_t const numPages = allocSize / pageSize;

    SegmentTable_Initialize(mountPoint);
    PageTable_Initialize(numPages);

    LISSANDRA_LOG_INFO("Memoria inicializada. Tamaño: %d bytes (%d páginas). Tamaño de página: %d", allocSize, numPages, pageSize);
}

uint32_t Memory_GetMaxValueLength(void)
{
    return MaxValueLength;
}

bool Memory_IsFull(void)
{
    //return SegmentTable_IsFull();
    return false;
}

void Memory_Destroy(void)
{
    PageTable_Destroy();
    SegmentTable_Destroy();
    Free(Memory);
}
