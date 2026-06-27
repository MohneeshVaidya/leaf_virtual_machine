#include <stdio.h>
#include <stdlib.h>


#include "memory.h"


void *reallocate(void *pointer, size_t size) {
    if (size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, size);
    if (result == NULL) {
        fprintf(stderr, "not able to allocate memory for the program\n");
        exit(1);
    }

    return result;
}
