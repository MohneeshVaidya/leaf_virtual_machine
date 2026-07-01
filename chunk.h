#ifndef leaf_chunk_h
#define leaf_chunk_h


#include <stdint.h>
#include <stddef.h>


#include "table.h"


typedef enum Operation {
    OP_CONSTANT,
    OP_PRINT,
    OP_PRINTLN,
    OP_COMMA,
    OP_TERNARY,
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
    OP_AND,
    OP_OR,
    OP_NEGATE,
    OP_NOP,
    OP_NOT,
    OP_RETURN,
    OP_EXIT,
} Operation;


typedef struct Chunk {
    size_t capacity;
    size_t count;
    uint8_t *code;
    int *lines;

    Value constantPool[256];
    int poolTop;
} Chunk;


void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void chunkAdd(Chunk *chunk, uint8_t byte, int line);
int addConstant(Chunk *chunk, Value constant);


#endif
