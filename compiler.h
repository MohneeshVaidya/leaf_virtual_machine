#ifndef leaf_compiler_h
#define leaf_compiler_h


#include <stdbool.h>


#include "chunk.h"


bool compile(const char *source, Chunk *chunk);


#endif
