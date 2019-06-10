
#include "MainMemory.h"
#include "PageTable.h"
#include "SegmentTable.h"
#include <libcommons/bitarray.h>
#include <Config.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdint.h>
#include <Timer.h>

// la memoria es un arreglo de páginas contiguas
static Frame* Memory = NULL;
static uint32_t MaxValueLength = 0;
static size_t FrameSize = 0;
static size_t NumPages = 0;

// bitmap de frames libres
static uint8_t* FrameBitmap = NULL;
static t_bitarray* FrameStatus = NULL;

static void WriteFrame(size_t frameNumber, uint16_t key, char const* value);
static size_t GetFreeFrame(void);

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
    FrameSize = sizeof(Frame) + maxValueLength;
    NumPages = allocSize / FrameSize;

    SegmentTable_Initialize(mountPoint);

    size_t bitmapBytes = NumPages / 8;
    if (NumPages % 8)
        ++bitmapBytes;

    FrameBitmap = Calloc(bitmapBytes, 1);
    FrameStatus = bitarray_create_with_mode((char*) FrameBitmap, bitmapBytes, MSB_FIRST);

    LISSANDRA_LOG_INFO("Memoria inicializada. Tamaño: %d bytes (%d páginas). Tamaño de página: %d", allocSize, NumPages, FrameSize);
}

void Memory_SaveNewValue(char const* tableName, uint16_t key, char const* value)
{
    size_t i = GetFreeFrame();

    PageTable* pt = SegmentTable_GetPageTable(tableName);
    if (!pt)
        pt = SegmentTable_CreateSegment(tableName);

    PageTable_AddPage(pt, key, i);
    WriteFrame(i, key, value);
}

void Memory_UpdateValue(char const* tableName, uint16_t key, char const* value)
{
    PageTable* pt = SegmentTable_GetPageTable(tableName);
    Frame* f = NULL;
    if (pt)
        f = PageTable_GetFrame(pt, key);

    if (!f)
    {
        size_t i = GetFreeFrame();
        pt = SegmentTable_GetPageTable(tableName);
        if (!pt)
            pt = SegmentTable_CreateSegment(tableName);

        PageTable_AddPage(pt, key, i);
    }

    size_t const frameOffset = f - Memory;
    WriteFrame(frameOffset, key, value);

    PageTable_MarkDirty(pt, key);
}

void Memory_CleanFrame(size_t frameNumber)
{
    bitarray_clean_bit(FrameStatus, frameNumber);
}

void Memory_EvictPages(char const* tableName)
{
    PageTable* pt = SegmentTable_GetPageTable(tableName);
    if (!pt)
        return;

    PageTable_Clean(pt);
    SegmentTable_DeleteSegment(tableName);
}

Frame* Memory_GetFrame(char const* tableName, uint16_t key)
{
    PageTable* pt = SegmentTable_GetPageTable(tableName);
    if (!pt)
        return NULL;

    return PageTable_GetFrame(pt, key);
}

uint32_t Memory_GetMaxValueLength(void)
{
    return MaxValueLength;
}

Frame* Memory_Read(size_t frameNumber)
{
    return ((Frame*) ((uint8_t*) Memory) + frameNumber * FrameSize);
}

void Memory_DoJournal(void)
{

}

void Memory_Destroy(void)
{
    bitarray_destroy(FrameStatus);
    Free(FrameBitmap);
    SegmentTable_Destroy();
    Free(Memory);
}

static void WriteFrame(size_t frameNumber, uint16_t key, char const* value)
{
    Frame* const f = Memory_Read(frameNumber);
    f->Key = key;
    f->Timestamp = GetMSTime();
    memcpy(f->Value, value, MaxValueLength);
}

static size_t GetFreeFrame(void)
{
    size_t i = 0;
    for (; i < NumPages; ++i)
        if (!bitarray_test_bit(FrameStatus, i))
            break;

    if (i == NumPages)
    {
        // no encontre ninguno libre, desalojar el LRU
        size_t freeFrame;
        if (SegmentTable_GetLRUFrame(&freeFrame))
            i = freeFrame;
        else
        {
            // estoy full
            Memory_DoJournal();

            // el primero disponible es el elemento 0
            i = 0;
        }
    }
    else
        bitarray_set_bit(FrameStatus, i);

    return i;
}
