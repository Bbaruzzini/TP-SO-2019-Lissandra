
#include "PageTable.h"
#include <libcommons/bitarray.h>
#include <Malloc.h>
#include <stdint.h>

// la tabla de paginas solo es un bitfield de dirty flags
// ya que las paginas tienen longitud fija y se acceden por offset.
static uint8_t* BitBuffer = NULL;
static t_bitarray* PageTable = NULL;

void PageTable_Initialize(size_t numPages)
{
    // el array deberia contener numPages bits como mínimo
    size_t bitBuffSize = numPages / 8;
    if (numPages % 8)
        ++bitBuffSize;

    BitBuffer = Calloc(bitBuffSize, sizeof(uint8_t));
    PageTable = bitarray_create_with_mode(BitBuffer, bitBuffSize, LSB_FIRST);
}

void PageTable_Destroy(void)
{
    bitarray_destroy(PageTable);
    Free(BitBuffer);
}
