#ifndef leaf_chunk_h
#define leaf_chunk_h


#include "forward.h"


void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void chunkAdd(Chunk *chunk, uint8_t byte, int line);
int addConstant(Chunk *chunk, Value constant);
void setDebugLocal(Chunk *chunk, int index, ObjString *name);


#endif
