
#ifndef MainMemory_h__
#define MainMemory_h__

#include <stdbool.h>
#include <stdint.h>

void Memory_Init(uint32_t maxValueLength);

bool Memory_IsFull(void);

void Memory_Terminate(void);

#endif //MainMemory_h__
