#ifndef leaf_chunk_h
#define leaf_chunk_h


#include <stdint.h>
#include <stddef.h>


typedef enum Operation {
    OP_CONSTANT,
    OP_PRINT,
    OP_PRINTLN,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESSER,
    OP_LESSER_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_RETURN,
} Operation;


typedef struct Chunk {
    size_t capacity;
    size_t count;
    uint8_t *code;
} Chunk;


void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void chunkAdd(Chunk *chunk, uint8_t byte);


#endif
