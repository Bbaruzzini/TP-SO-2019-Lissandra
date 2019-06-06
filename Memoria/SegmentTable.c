
#include "SegmentTable.h"
#include <libcommons/dictionary.h>
#include <libcommons/string.h>
#include <Malloc.h>

// la tabla de segmentos es una tabla de hash, accedida por 'path de la tabla'.
// cada segmento es una lista enlazada simple de las páginas que componen el segmento
static t_dictionary* SegmentTable = NULL;
static char* TablePath = NULL;

void SegmentTable_Initialize(char const* tablePath)
{
    SegmentTable = dictionary_create();
    TablePath = string_duplicate(tablePath);
}

Segment* SegmentTable_GetSegment(char const* tableName)
{
    char* key = string_duplicate(TablePath);
    string_append_with_format(&key, "%s", tableName);

    Segment* s = dictionary_get(SegmentTable, key);

    Free(key);
    return s;
}

void SegmentTable_Destroy(void)
{
    Free(TablePath);
    dictionary_destroy_and_destroy_elements(SegmentTable, Free);
}
