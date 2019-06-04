
#include "SegmentTable.h"
#include <libcommons/dictionary.h>
#include <libcommons/list.h>
#include <Malloc.h>

typedef struct
{
    t_list* Pages;
} SegmentDescriptor;

// la tabla de segmentos es una tabla de hash, accedida por 'path de la tabla'.
// cada segmento es una lista enlazada simple de las páginas que componen el segmento
static t_dictionary* SegmentTable = NULL;

void SegmentTable_Initialize(void)
{
    SegmentTable = dictionary_create();
}

void SegmentTable_Destroy(void)
{
    dictionary_destroy_and_destroy_elements(SegmentTable, Free);
}
