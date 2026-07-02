#ifndef leaf_chunk_h
#define leaf_chunk_h


#include <stdint.h>
#include <stddef.h>


#include "table.h"


typedef enum Operation {
    OP_CONSTANT,
    OP_POP,
    OP_PRINT,
    OP_PRINTLN,
    OP_COMMA,
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
    OP_NEGATE,
    OP_POS,
    OP_NOT,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_DECLARE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_GLOBAL,
    OP_RETURN,
    OP_NOP,
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
