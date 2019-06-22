
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
    bool Valid;
} Page;

static Page* _findPageByKey(PageTable const* pt, uint16_t key);

typedef struct
{
    size_t* minCounter;
    Page** minPage;
}  SLRUPageParameters;
static void _saveLRUPage(void* page, void* param);

typedef struct
{
    char const* tableName;
    Vector* dirtyFrames;
} GDFrameParameters;
static void _addDirtyFrame(void* page, void* param);

static void _cleanPage(void* page);

void PageTable_Construct(PageTable* pt, size_t totalFrames)
{
    pt->UsedPages = 0;
    Vector_Construct(&pt->PageEntries, sizeof(Page), _cleanPage, totalFrames);
    Vector_resize_zero(&pt->PageEntries, totalFrames);
}

void PageTable_AddPage(PageTable* pt, uint16_t key, size_t frame)
{
    Page* p = Vector_at(&pt->PageEntries, frame);
    p->Key = key;
    p->Counter = ++MonotonicCounter;
    p->Frame = frame;
    p->Dirty = false;
    p->Valid = true;

    ++pt->UsedPages;
}

bool PageTable_GetLRUFrame(PageTable const* pt, size_t* frame, size_t* timestamp)
{
    size_t minCounter = 0;
    Page* minPage = NULL;

    SLRUPageParameters p =
    {
        .minCounter = &minCounter,
        .minPage = &minPage
    };
    Vector_iterate_with_data(&pt->PageEntries, _saveLRUPage, &p);

    if (!minPage)
        return false;

    *timestamp = minPage->Counter;
    *frame = minPage->Frame;

    return true;
}

void PageTable_GetDirtyFrames(PageTable const* pt, char const* tableName, Vector* dirtyFrames)
{
    GDFrameParameters p =
    {
        .tableName = tableName,
        .dirtyFrames = dirtyFrames
    };

    Vector_iterate_with_data(&pt->PageEntries, _addDirtyFrame, &p);
}

bool PageTable_PreemptPage(PageTable* pt, uint16_t key)
{
    Page* p = _findPageByKey(pt, key);
    if (p)
    {
        _cleanPage(p);
        --pt->UsedPages;
    }

    return pt->UsedPages == 0;
}

bool PageTable_GetFrameNumber(PageTable const* pt, uint16_t key, size_t* page)
{
    Page* p = _findPageByKey(pt, key);
    if (!p)
        return false;

    p->Counter = ++MonotonicCounter;
    *page = p->Frame;
    return true;
}

void PageTable_MarkDirty(PageTable const* pt, uint16_t key)
{
    Page* p = _findPageByKey(pt, key);
    if (!p)
        return;

    p->Dirty = true;
}

void PageTable_Destruct(PageTable* pt)
{
    Vector_Destruct(&pt->PageEntries);
}

/* PRIVATE */
static Page* _findPageByKey(PageTable const* pt, uint16_t key)
{
    Page* pages = Vector_data(&pt->PageEntries);
    Page* res = NULL;

    for (size_t i = 0; i < Vector_size(&pt->PageEntries); ++i)
    {
        if (!pages[i].Valid)
            continue;

        if (pages[i].Key == key)
        {
            res = pages + i;
            break;
        }
    }

    return res;
}

static void _saveLRUPage(void* page, void* param)
{
    Page* const p = page;
    if (!p->Valid)
        return;

    SLRUPageParameters* const data = param;
    if (!p->Dirty && (!*data->minPage || p->Counter < *data->minCounter))
    {
        *data->minCounter = p->Counter;
        *data->minPage = p;
    }
}

static void _addDirtyFrame(void* page, void* param)
{
    Page* const p = page;
    if (!p->Valid || !p->Dirty)
        return;

    Frame* f = Memory_Read(p->Frame);
    GDFrameParameters* const data = param;
    DirtyFrame df =
    {
        .TableName = data->tableName,
        .Timestamp = f->Timestamp,
        .Key = f->Key,
        .Value = f->Value
    };

    Vector_push_back(data->dirtyFrames, &df);
}

static void _cleanPage(void* page)
{
    Page* const p = page;
    Memory_CleanFrame(p->Frame);
    p->Valid = false;
}
