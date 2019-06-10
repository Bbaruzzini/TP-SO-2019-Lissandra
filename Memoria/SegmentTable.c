
#include "SegmentTable.h"
#include "MainMemory.h"
#include <libcommons/list.h>
#include <linux/limits.h>
#include <Malloc.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    char* Table;
    PageTable Pages;
} Segment;

static t_list* SegmentTable = NULL;
static char* TablePath = NULL;

static void _segmentDestroy(void* segment);

static bool FindSegmentPred(void* segment, void* tablePath)
{
    Segment* const s = segment;
    return !strcmp(s->Table, (char*) tablePath);
}

void SegmentTable_Initialize(char const* tablePath)
{
    SegmentTable = list_create();
    TablePath = strdup(tablePath);
}

PageTable* SegmentTable_CreateSegment(char const* tableName)
{
    char tablePath[PATH_MAX];
    snprintf(tablePath, PATH_MAX, "%s%s", TablePath, tableName);

    Segment* s = Malloc(sizeof(Segment));
    s->Table = strdup(tablePath);
    PageTable_Construct(&s->Pages);

    list_add(SegmentTable, s);
    return &s->Pages;
}

PageTable* SegmentTable_GetPageTable(char const* tableName)
{
    char tablePath[PATH_MAX];
    snprintf(tablePath, PATH_MAX, "%s%s", TablePath, tableName);

    Segment* s = list_find(SegmentTable, FindSegmentPred, tablePath);
    if (!s)
        return NULL;

    return &s->Pages;
}

struct Param
{
    size_t* minCounter;
    size_t* minFrame;
    Segment** minPageSegment;
};

static void SaveLRUPage(void* segment, void* param)
{
    Segment* const s = segment;

    size_t lruFrame;
    size_t timestamp;
    bool hasLRU = PageTable_GetLRUFrame(&s->Pages, &lruFrame, &timestamp);

    struct Param* const p = param;
    if (hasLRU && (!*p->minPageSegment || timestamp < *p->minCounter))
    {
        *p->minCounter = timestamp;
        *p->minFrame = lruFrame;
        *p->minPageSegment = s;
    }
}

bool SegmentTable_GetLRUFrame(size_t* frame)
{
    if (list_is_empty(SegmentTable))
        return false;

    // entre los segmentos busco la pagina con menor timestamp
    size_t minCounter = 0;
    size_t minFrame = 0;
    Segment* minPageSegment = NULL;

    struct Param p =
    {
        .minCounter = &minCounter,
        .minFrame = &minFrame,
        .minPageSegment = &minPageSegment
    };
    list_iterate_with_data(SegmentTable, SaveLRUPage, &p);

    if (!minPageSegment)
        return false;

    *frame = minFrame;

    Frame* f = Memory_Read(minFrame);
    if (PageTable_PreemptPage(&minPageSegment->Pages, f->Key))
    {
        // era la ultima pagina de este segmento, borrarlo
        list_remove_and_destroy_by_condition(SegmentTable, FindSegmentPred, minPageSegment->Table, _segmentDestroy);
    }

    return true;
}

static void GetDirtyFrames(void* segment, void* vec)
{
    Segment* const s = segment;
    PageTable_GetDirtyFrames(&s->Pages, s->Table, (Vector*) vec);
}

void SegmentTable_GetDirtyFrames(Vector* dirtyFrames)
{
    list_iterate_with_data(SegmentTable, GetDirtyFrames, dirtyFrames);
}

static void _segmentDestroy(void* segment)
{
    Segment* const s = segment;
    Free(s->Table);
    PageTable_Destruct(&s->Pages);
    Free(s);
}

void SegmentTable_DeleteSegment(char const* tableName)
{
    list_remove_and_destroy_by_condition(SegmentTable, FindSegmentPred, tableName, _segmentDestroy);
}

void SegmentTable_Clean(void)
{
    list_clean_and_destroy_elements(SegmentTable, _segmentDestroy);
}

void SegmentTable_Destroy(void)
{
    Free(TablePath);
    list_destroy_and_destroy_elements(SegmentTable, _segmentDestroy);
}
