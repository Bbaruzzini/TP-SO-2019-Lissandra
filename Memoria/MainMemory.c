
#include "MainMemory.h"
#include "API.h"
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
    size_t allocSize = config_get_long_value(sConfig, "TAM_MEM");

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
    size_t i;
    bool res = false;
    if (pt)
        res = PageTable_GetFrameNumber(pt, key, &i);

    if (!res)
    {
        i = GetFreeFrame();
        pt = SegmentTable_GetPageTable(tableName);
        if (!pt)
            pt = SegmentTable_CreateSegment(tableName);

        PageTable_AddPage(pt, key, i);
    }

    WriteFrame(i, key, value);
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

    size_t frame;
    if (!PageTable_GetFrameNumber(pt, key, &frame))
        return NULL;

    return Memory_Read(frame);
}

uint32_t Memory_GetMaxValueLength(void)
{
    return MaxValueLength;
}

Frame* Memory_Read(size_t frameNumber)
{
    return ((Frame*) ((uint8_t*) Memory) + frameNumber * FrameSize);
}

void Memory_DoJournal(void(*insertFn)(void*))
{
    Vector v;
    Vector_Construct(&v, sizeof(DirtyFrame), NULL, 0);
    SegmentTable_GetDirtyFrames(&v);

    Vector_iterate(&v, insertFn);

    memset(FrameBitmap, 0, bitarray_get_max_bit(FrameStatus) / 8);
    SegmentTable_Clean();

    Vector_Destruct(&v);
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
    strncpy(f->Value, value, MaxValueLength);
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
            API_Journal();

            // el primero disponible es el elemento 0
            i = 0;
        }
    }

    bitarray_set_bit(FrameStatus, i);
    return i;
}
