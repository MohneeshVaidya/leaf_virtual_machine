#include "chunk.h"


void initChunk(Chunk *chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
}


void freeChunk(Chunk *chunk) {

}
