#ifndef leaf_vm_h
#define leaf_vm_h


typedef enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_COMPILE_ERROR,
} InterpretResult;


InterpretResult interpret(const char *source);


#endif
