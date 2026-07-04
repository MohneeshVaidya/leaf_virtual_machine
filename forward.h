#ifndef leaf_forward_h
#define leaf_forward_h


#include <stdint.h>
#include <stdbool.h>


typedef struct Local Local;
typedef struct Scope Scope;
typedef struct VM VM;

typedef struct Value Value;

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef struct Chunk Chunk;

typedef struct Entry Entry;
typedef struct Table Table;
typedef struct ObjFunction ObjFunction;

typedef struct CallFrame CallFrame;

typedef struct UpValue UpValue;


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
    OP_DECLARE_LOCAL,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_SET_UPVALUE,
    OP_GET_UPVALUE,
    OP_CALL,
    OP_RETURN,
    OP_NOP,
    OP_EXIT,
} Operation;


typedef enum ValueType {
    VALUE_NUMBER,
    VALUE_BOOLEAN,
    VALUE_NIL,
    VALUE_TOMBSTONE,
    VALUE_OBJ,
} ValueType;


struct Value {
    ValueType type;
    union {
        double number;
        bool boolean;
        Obj *obj;
    } as;
};

struct Table {
    size_t capacity;
    size_t count;
    Entry *entries;
};


typedef struct Chunk {
    size_t capacity;
    size_t count;
    uint8_t *code;
    int *lines;
    ObjString *debugLocalPool[256];
    Value constantPool[256];
    int poolTop;
} Chunk;


typedef enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_COMPILE_ERROR,
} InterpretResult;


struct Entry {
    ObjString *key;
    Value value;
};


typedef enum ObjType {
    OBJ_STRING,
    OBJ_FUNCTION,
} ObjType;


struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj *next;
};


struct ObjString {
    Obj meta;
    char *chars;
    size_t length;
    uint32_t hash;
};


struct UpValue {
    ObjString *name;
    Value value;
};


struct ObjFunction {
    Obj meta;
    ObjString *name;
    int arity;
    ObjString *parameters[256];
    Chunk chunk;
    UpValue upvalues[256];
    bool upvaluesFilled;
};


struct CallFrame {
    ObjFunction *function;
    uint8_t *ip;
    Value *frameStack;
};


struct VM {
    // Chunk *chunk;
    // uint8_t *ip;

    Table globals;
    Table strings;

    Obj *objects;

    Value stack[256];
    int stackTop;

    CallFrame frames[10240];
    int framesTop;
};

#endif
