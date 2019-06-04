
#ifndef MainMemory_h__
#define MainMemory_h__

#include <stdbool.h>
#include <stdint.h>

void Memory_Initialize(uint32_t maxValueLength);

bool Memory_IsFull(void);

void Memory_Destroy(void);

#endif //MainMemory_h__
