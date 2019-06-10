
#include "PageTable.h"
#include "MainMemory.h"
#include <Malloc.h>

// contador de uso, común a todas las tablas de página
static size_t MonotonicCounter = 0;

typedef struct
{
    uint16_t Key;
    size_t Counter;
    size_t Frame;
    bool Dirty;
} Page;

static bool FindPagePred(void* page, void* key)
{
    Page* const p = page;
    return p->Key == (uint16_t) key;
}

void PageTable_Construct(PageTable* pt)
{
    pt->Frames = list_create();
}

void PageTable_AddPage(PageTable* pt, uint16_t key, size_t frame)
{
    Page* p = Malloc(sizeof(Page));
    p->Key = key;
    p->Counter = ++MonotonicCounter;
    p->Frame = frame;
    p->Dirty = false;

    list_add(pt->Frames, p);
}

struct ParamLRU
{
    size_t* minCounter;
    Page** minPage;
};

static void SaveLRUPage(void* page, void* param)
{
    Page* const p = page;

    struct ParamLRU* const data = param;
    if (!p->Dirty && (!*data->minPage || p->Counter < *data->minCounter))
    {
        *data->minCounter = p->Counter;
        *data->minPage = p;
    }
}

bool PageTable_GetLRUFrame(PageTable const* pt, size_t* frame, size_t* timestamp)
{
    if (list_is_empty(pt->Frames))
        return false;

    size_t minCounter = 0;
    Page* minPage = NULL;

    struct ParamLRU p =
    {
        .minCounter = &minCounter,
        .minPage = &minPage
    };
    list_iterate_with_data(pt->Frames, SaveLRUPage, &p);

    if (!minPage)
        return false;

    *timestamp = minPage->Counter;
    *frame = minPage->Counter;

    return true;
}

struct ParamDF
{
    char const* tableName;
    Vector* dirtyFrames;
};

void AddDirtyFrame(void* page, void* param)
{
    Page* const p = page;
    if (!p->Dirty)
        return;

    Frame* f = Memory_Read(p->Frame);
    struct ParamDF* const data = param;
    DirtyFrame df =
    {
        .TableName = data->tableName,
        .Timestamp = f->Timestamp,
        .Key = f->Key,
        .Value = f->Value
    };

    Vector_push_back(data->dirtyFrames, &df);
}

void PageTable_GetDirtyFrames(PageTable const* pt, char const* tableName, Vector* dirtyFrames)
{
    struct ParamDF p =
    {
        .tableName = tableName,
        .dirtyFrames = dirtyFrames
    };

    list_iterate_with_data(pt->Frames, AddDirtyFrame, &p);
}

bool PageTable_PreemptPage(PageTable* pt, uint16_t key)
{
    list_remove_and_destroy_by_condition(pt->Frames, FindPagePred, (void*) key, Free);
    return list_is_empty(pt->Frames);
}

Frame* PageTable_GetFrame(PageTable const* pt, uint16_t key)
{
    Page* p = list_find(pt->Frames, FindPagePred, (void*) key);
    if (!p)
        return NULL;

    p->Counter = ++MonotonicCounter;
    return Memory_Read(p->Frame);
}

void PageTable_MarkDirty(PageTable const* pt, uint16_t key)
{
    Page* p = list_find(pt->Frames, FindPagePred, (void*) key);
    if (!p)
        return;

    p->Dirty = true;
}

static void CleanPage(void* page)
{
    Page* const p = page;
    Memory_CleanFrame(p->Frame);
    Free(p);
}

void PageTable_Clean(PageTable* pt)
{
    list_clean_and_destroy_elements(pt->Frames, CleanPage);
}

void PageTable_Destruct(PageTable* pt)
{
    list_destroy_and_destroy_elements(pt->Frames, Free);
}
