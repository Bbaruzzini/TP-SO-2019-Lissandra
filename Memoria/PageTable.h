
#ifndef PageTable_h__
#define PageTable_h__

#include "Frame.h"
#include <libcommons/bitarray.h>
#include <vector.h>

typedef struct
{
    size_t UsedPages;
    Vector PageEntries;
} PageTable;

void PageTable_Construct(PageTable* pt, size_t totalFrames);

void PageTable_AddPage(PageTable* pt, uint16_t key, size_t frame);

bool PageTable_GetLRUFrame(PageTable const* pt, size_t* frame, size_t* timestamp);

void PageTable_GetDirtyFrames(PageTable const* pt, char const* tableName, Vector* dirtyFrames);

bool PageTable_GetFrameNumber(PageTable const* pt, uint16_t key, size_t* page);

void PageTable_MarkDirty(PageTable const* pt, uint16_t key);

bool PageTable_PreemptPage(PageTable* pt, uint16_t key);

void PageTable_Destruct(PageTable* pt);

#endif //PageTable_h__
