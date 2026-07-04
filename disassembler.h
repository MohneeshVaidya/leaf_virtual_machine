#ifndef leaf_disassembler_h
#define leaf_disassembler_h


#include "forward.h"


// #define PRINT_PER_INSTRUNCTION_ASSEMBLY
#define PRINT_ASSEMBLY


void disassemble(ObjClosure *closure);
size_t disassembleInstruction(size_t offset, ObjClosure *closure);


#endif
