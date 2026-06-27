#include "vm.h"
#include "compiler.h"


InterpretResult interpret(const char *source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        return INTERPRET_COMPILE_ERROR;
    }

    return INTERPRET_OK;
}
