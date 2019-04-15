
#ifndef MessageBuffer_h__
#define MessageBuffer_h__

#include "vector.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
    size_t _wpos, _rpos;
    Vector _storage;
} MessageBuffer;

static inline void MessageBuffer_Init(MessageBuffer* mb, size_t initialSize)
{
    assert(initialSize);

    mb->_wpos = 0;
    mb->_rpos = 0;
    Vector_Construct(&mb->_storage, sizeof(uint8_t), NULL, 0);
    Vector_resize_zero(&mb->_storage, initialSize);
}

static inline void MessageBuffer_Reset(MessageBuffer* mb)
{
    mb->_wpos = 0;
    mb->_rpos = 0;
}

static inline void MessageBuffer_Resize(MessageBuffer* mb, size_t bytes)
{
    Vector_resize_zero(&mb->_storage, bytes);
}

static inline uint8_t* MessageBuffer_GetBasePointer(MessageBuffer* mb)
{
    return (uint8_t*) Vector_data(&mb->_storage);
}

static inline uint8_t* MessageBuffer_GetReadPointer(MessageBuffer* mb)
{
    return MessageBuffer_GetBasePointer(mb) + mb->_rpos;
}

static inline uint8_t* MessageBuffer_GetWritePointer(MessageBuffer* mb)
{
    return MessageBuffer_GetBasePointer(mb) + mb->_wpos;
}

static inline void MessageBuffer_ReadCompleted(MessageBuffer* mb, size_t bytes)
{
    mb->_rpos += bytes;
}

static inline void MessageBuffer_WriteCompleted(MessageBuffer* mb, size_t bytes)
{
    mb->_wpos += bytes;
}

static inline size_t MessageBuffer_GetActiveSize(MessageBuffer const* mb)
{
    return mb->_wpos - mb->_rpos;
}

static inline size_t MessageBuffer_GetRemainingSpace(MessageBuffer const* mb)
{
    return Vector_size(&mb->_storage) - mb->_wpos;
}

static inline size_t MessageBuffer_GetBufferSize(MessageBuffer const* mb)
{
    return Vector_size(&mb->_storage);
}

// Discards inactive data
static inline void MessageBuffer_Normalize(MessageBuffer* mb)
{
    if (mb->_rpos)
    {
        if (mb->_rpos != mb->_wpos)
            memmove(MessageBuffer_GetBasePointer(mb), MessageBuffer_GetReadPointer(mb),
                    MessageBuffer_GetActiveSize(mb));
        mb->_wpos -= mb->_rpos;
        mb->_rpos = 0;
    }
}

// Ensures there's "some" Free space, make sure to call Normalize() before this
static inline void MessageBuffer_EnsureFreeSpace(MessageBuffer* mb)
{
    // resize buffer if it's already full
    if (MessageBuffer_GetRemainingSpace(mb) == 0)
        Vector_resize_zero(&mb->_storage, Vector_size(&mb->_storage) * 3 / 2);
}

static inline void MessageBuffer_Write(MessageBuffer* mb, void const* data, size_t size)
{
    if (size)
    {
        memcpy(MessageBuffer_GetWritePointer(mb), data, size);
        MessageBuffer_WriteCompleted(mb, size);
    }
}

static inline void MessageBuffer_SwapStorage(MessageBuffer* mb, Vector* v)
{
    mb->_wpos = 0;
    mb->_rpos = 0;
    Vector_swap(&mb->_storage, v);
}

static inline void MessageBuffer_Destroy(MessageBuffer* mb)
{
    Vector_Destruct(&mb->_storage);
}

#endif //MessageBuffer_h__
