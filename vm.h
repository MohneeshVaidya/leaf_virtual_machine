#ifndef leaf_vm_h
#define leaf_vm_h


#include "chunk.h"
#include "table.h"


typedef enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_COMPILE_ERROR,
} InterpretResult;


typedef struct VM {
    Chunk *chunk;
    uint8_t *ip;

    Table strings;
    Obj *objects;

    Value stack[256];
    int stackTop;
} VM;

extern VM vm;


InterpretResult interpret(const char *source);
void push(Value value);
Value pop();
Value top();


#endif
