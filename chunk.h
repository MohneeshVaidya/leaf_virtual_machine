#ifndef leaf_chunk_h
#define leaf_chunk_h


#include <stdint.h>
#include <stddef.h>


typedef struct Chunk {
    size_t capacity;
    size_t count;
    uint8_t *code;
} Chunk;


void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);


#endif
