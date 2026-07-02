#ifndef leaf_vm_h
#define leaf_vm_h


#include "chunk.h"
#include "table.h"


typedef enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_COMPILE_ERROR,
} InterpretResult;


typedef struct Local {
    ObjString *name;
    int depth;
} Local;


typedef struct Scope {
    Local locals[256];
    int scopeDepth;
} Scope;


typedef struct VM {
    Chunk *chunk;
    uint8_t *ip;

    Scope scope;

    Table globals;
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
