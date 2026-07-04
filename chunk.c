#include <stdio.h>
#include <stdlib.h>


#include "chunk.h"
#include "memory.h"


void initChunk(Chunk *chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->poolTop = 0;
    for (int i = 0; i < 256; i++) {
        chunk->debugLocalPool[i] = NULL;
    }
}


void freeChunk(Chunk *chunk) {
    FREE(uint8_t, chunk->code, chunk->capacity);
    FREE(int, chunk->lines, chunk->capacity);
}


void chunkAdd(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->count >= chunk->capacity) {
        size_t capacity = GROW_CAPACITY(chunk->capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, chunk->capacity, capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, chunk->capacity, capacity);
        chunk->capacity = capacity;
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}


int addConstant(Chunk *chunk, Value value) {
    if (chunk->poolTop >= 256) {
        fprintf(stderr, "constant pool overflow\n");
        exit(1);
    }

    chunk->constantPool[chunk->poolTop] = value;
    return chunk->poolTop++;
}


void setDebugLocal(Chunk *chunk, int index, ObjString *name) {
    if (index < 0 || index >= 256) {
        fprintf(stderr, "can not set index '%d' in debugLocalPool\n", index);
        exit(1);
    }
    chunk->debugLocalPool[index] = name;
}
