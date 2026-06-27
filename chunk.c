#include "chunk.h"
#include "memory.h"


void initChunk(Chunk *chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
}


void freeChunk(Chunk *chunk) {
    FREE(uint8_t, chunk->code);
    initChunk(chunk);
}


void chunkAdd(Chunk *chunk, uint8_t byte) {
    if (chunk->count >= chunk->capacity) {
        size_t capacity = GROW_CAPACITY(chunk->capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, capacity);
        chunk->capacity = capacity;
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}
