#include <stdio.h>


#include "disassembler.h"
#include "chunk.h"
#include "value.h"
#include "vm.h"


static size_t simpleInstruction(const char *operation, size_t offset) {
    printf("%s", operation);
    return offset + 1;
}


static size_t constantInstruction(const char *operation, size_t offset, Chunk *chunk) {
    int constantIndex = chunk->code[offset + 1];

    printf("%-16s %d ", operation, constantIndex);
    printf("'");
    printValue(chunk->constantPool[constantIndex]);
    printf("'");
    return offset + 2;
}


static size_t jumpInstruction(const char *operation, size_t offset, Chunk *chunk) {
    size_t jumpTo = (size_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
    printf("%-16s -> %04llu ", operation, jumpTo);
    return offset + 3;
}


static size_t localInstruction(const char *operation, size_t offset, Chunk *chunk) {
    int localIndex = chunk->code[offset + 1];

    printf("%-16s %d ", operation, localIndex);
    printf("'");
    printValue(OBJ_VALUE(vm.scope.locals[localIndex].name));
    printf("'");
    return offset + 2;
}


size_t disassembleInstruction(size_t offset, Chunk *chunk) {
    printf("%04llu ", offset);

    if (offset > 0 && chunk->lines[offset - 1] == chunk->lines[offset]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    Operation operation = chunk->code[offset];

    switch (operation) {
        case OP_CONSTANT: return constantInstruction("OP_CONSTANT", offset, chunk);
        case OP_POP: return simpleInstruction("OP_POP", offset);
        case OP_COMMA: return simpleInstruction("OP_COMMA", offset);
        case OP_ADD: return simpleInstruction("OP_ADD", offset);
        case OP_SUB: return simpleInstruction("OP_SUB", offset);
        case OP_MUL: return simpleInstruction("OP_MUL", offset);
        case OP_DIV: return simpleInstruction("OP_DIV", offset);
        case OP_MOD: return simpleInstruction("OP_MOD", offset);
        case OP_POW: return simpleInstruction("OP_POW", offset);
        case OP_EQUAL: return simpleInstruction("OP_EQUAL", offset);
        case OP_NOT_EQUAL:return simpleInstruction("OP_NOT_EQUAL", offset);
        case OP_LESSER: return simpleInstruction("OP_LESSER", offset);
        case OP_LESSER_EQUAL:return simpleInstruction("OP_LESSER_EQUAL", offset);
        case OP_GREATER: return simpleInstruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL: return simpleInstruction("OP_GREATER_EQUAL", offset);
        case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);
        case OP_NOT: return simpleInstruction("OP_NOT", offset);
        case OP_POS: return simpleInstruction("OP_POS", offset);
        case OP_PRINT: return simpleInstruction("OP_PRINT", offset);
        case OP_PRINTLN: return simpleInstruction("OP_PRINTLN", offset);
        case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", offset, chunk);
        case OP_JUMP: return jumpInstruction("OP_JUMP", offset, chunk);
        case OP_DECLARE_GLOBAL: return constantInstruction("OP_DECLARE_GLOBAL", offset, chunk);
        case OP_GET_GLOBAL: return constantInstruction("OP_GET_GLOBAL", offset, chunk);
        case OP_SET_GLOBAL: return constantInstruction("OP_SET_GLOBAL", offset, chunk);
        case OP_DECLARE_LOCAL: return constantInstruction("OP_DECLARE_LOCAL", offset, chunk);
        case OP_SET_LOCAL: return localInstruction("OP_SET_LOCAL", offset, chunk);
        case OP_GET_LOCAL: return localInstruction("OP_GET_LOCAL", offset, chunk);
        case OP_NOP: return simpleInstruction("OP_NOP", offset);
        case OP_EXIT: return simpleInstruction("OP_EXIT", offset);
        default:
            break;
    }
    return offset;
}


void disassemble(Chunk *chunk) {
    printf("\n\n===== ASSEMBLY =====\n\n");

    size_t offset = 0;
    for (;offset < chunk->count;) {
        offset = disassembleInstruction(offset, chunk);
        printf("\n");
    }

    printf("\n\n");
}
