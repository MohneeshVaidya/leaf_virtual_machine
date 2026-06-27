#ifndef leaf_memory_h
#define leaf_memory_h


#include <stddef.h>


#define INITIAL_CAPACITY        (8)
#define GROW_CAPACITY(capacity) ((capacity < INITIAL_CAPACITY) ? INITIAL_CAPACITY : capacity * 2)


#define GROW_ARRAY(type, pointer, size) \
    ((type*)reallocate(pointer, size * sizeof(type)))


#define FREE(type, pointer) \
    ((type*)reallocate(pointer, 0))


void *reallocate(void *pointer, size_t size);


#endif
