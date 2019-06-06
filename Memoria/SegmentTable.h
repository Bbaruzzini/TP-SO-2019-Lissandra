
#ifndef SegmentTable_h__
#define SegmentTable_h__

#include <libcommons/list.h>

typedef struct
{
    t_list* Pages;
} Segment;

void SegmentTable_Initialize(char const* tablePath);

Segment* SegmentTable_GetSegment(char const* tableName);

void SegmentTable_Destroy(void);

#endif //SegmentTable_h__
