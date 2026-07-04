#include <stdio.h>


#include "disassembler.h"
#include "forward.h"
#include "value.h"


static size_t simpleInstruction(const char *operation, size_t offset) {
    printf("%s", operation);
    return offset + 1;
}


static size_t constantInstruction(const char *operation, size_t offset, ObjFunction *function) {
    int constantIndex = function->chunk.code[offset + 1];

    printf("%-16s %d ", operation, constantIndex);
    printf("'");
    printValue(function->chunk.constantPool[constantIndex]);
    printf("'");
    return offset + 2;
}


static size_t jumpInstruction(const char *operation, size_t offset, ObjFunction *function) {
    size_t jumpTo = (size_t)((function->chunk.code[offset + 1] << 8) | function->chunk.code[offset + 2]);
    printf("%-16s -> %04llu ", operation, jumpTo);
    return offset + 3;
}


static size_t localInstruction(const char *operation, size_t offset, ObjFunction *function) {
    int localIndex = function->chunk.code[offset + 1];

    printf("%-16s %d ", operation, localIndex);
    printf("'");
    printValue(OBJ_VALUE(function->chunk.debugLocalPool[localIndex]));
    printf("'");
    return offset + 2;
}


static size_t upvalueInstruction(const char *operation, size_t offset, ObjFunction *function) {
    int upvalueIndex = function->chunk.code[offset + 1];

    printf("%-16s %d ", operation, upvalueIndex);
    printf("'");
    printValue(OBJ_VALUE(function->upvalues[upvalueIndex].name));
    printf("'");
    return offset + 2;
}


static size_t callInstruction(const char *operation, size_t offset, ObjFunction *function) {
    int argumentCount = function->chunk.code[offset + 1];

    printf("%-16s %d arguments", operation, argumentCount);
    return offset + 2;
}


size_t disassembleInstruction(size_t offset, ObjFunction *function) {
    printf("%04llu ", offset);

    if (offset > 0 && function->chunk.lines[offset - 1] == function->chunk.lines[offset]) {
        printf("   | ");
    } else {
        printf("%4d ", function->chunk.lines[offset]);
    }

    Operation operation = function->chunk.code[offset];

    switch (operation) {
        case OP_CONSTANT: return constantInstruction("OP_CONSTANT", offset, function);
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
        case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", offset, function);
        case OP_JUMP: return jumpInstruction("OP_JUMP", offset, function);
        case OP_DECLARE_GLOBAL: return constantInstruction("OP_DECLARE_GLOBAL", offset, function);
        case OP_GET_GLOBAL: return constantInstruction("OP_GET_GLOBAL", offset, function);
        case OP_SET_GLOBAL: return constantInstruction("OP_SET_GLOBAL", offset, function);
        case OP_DECLARE_LOCAL: return constantInstruction("OP_DECLARE_LOCAL", offset, function);
        case OP_SET_LOCAL: return localInstruction("OP_SET_LOCAL", offset, function);
        case OP_GET_LOCAL: return localInstruction("OP_GET_LOCAL", offset, function);
        case OP_SET_UPVALUE: return upvalueInstruction("OP_SET_UPVALUE", offset, function);
        case OP_GET_UPVALUE: return upvalueInstruction("OP_GET_UPVALUE", offset, function);
        case OP_CALL: return callInstruction("OP_CALL", offset, function);
        case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
        case OP_NOP: return simpleInstruction("OP_NOP", offset);
        case OP_EXIT: return simpleInstruction("OP_EXIT", offset);
        default:
            break;
    }
    return offset;
}


void disassemble(ObjFunction *function) {
    printf("\n\n===== ASSEMBLY =====\n\n");

    size_t offset = 0;
    for (;offset < function->chunk.count;) {
        offset = disassembleInstruction(offset, function);
        printf("\n");
    }

    printf("\n\n");
}
