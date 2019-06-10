
#ifndef PageTable_h__
#define PageTable_h__

#include "Frame.h"
#include <libcommons/list.h>
#include <vector.h>

typedef struct
{
    t_list* Frames;
} PageTable;

void PageTable_Construct(PageTable* pt);

void PageTable_AddPage(PageTable* pt, uint16_t key, size_t frame);

bool PageTable_GetLRUFrame(PageTable const* pt, size_t* frame, size_t* timestamp);

void PageTable_GetDirtyFrames(PageTable const* pt, char const* tableName, Vector* dirtyFrames);

Frame* PageTable_GetFrame(PageTable const* pt, uint16_t key);

void PageTable_MarkDirty(PageTable const* pt, uint16_t key);

bool PageTable_PreemptPage(PageTable* pt, uint16_t key);

void PageTable_Clean(PageTable* pt);

void PageTable_Destruct(PageTable* pt);

#endif //PageTable_h__
