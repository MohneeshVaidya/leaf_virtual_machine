#ifndef leaf_memory_h
#define leaf_memory_h


#include <stddef.h>


#define INITIAL_CAPACITY        (8)
#define GROW_CAPACITY(capacity) ((capacity < INITIAL_CAPACITY) ? INITIAL_CAPACITY : capacity * 2)


void *reallocate(void *pointer, size_t oldSize, size_t newSize);


#endif
