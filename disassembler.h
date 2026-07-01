#ifndef leaf_disassembler_h
#define leaf_disassembler_h


#include "chunk.h"


#define PRINT_ASSEMBLY


void disassemble(Chunk *chunk);
size_t disassembleInstruction(size_t offset, Chunk *chunk);


#endif
