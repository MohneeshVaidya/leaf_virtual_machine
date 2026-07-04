#ifndef leaf_vm_h
#define leaf_vm_h


#include "forward.h"
#include "table.h"


extern VM vm;


InterpretResult interpret(const char *source);
void push(Value value);
Value pop();
Value top();
Value peek(int index);


#endif
