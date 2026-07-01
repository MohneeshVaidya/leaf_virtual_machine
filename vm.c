#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>


#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "operation.h"
#include "value.h"
#include "disassembler.h"


jmp_buf buf;


VM vm;


static void initVM() {
    vm.chunk = NULL;
    vm.ip = NULL;
    vm.objects = NULL;
    initTable(&vm.strings);
    vm.stackTop = 0;
}


static void freeVM() {
    vm.chunk = NULL;
    vm.ip = NULL;
    vm.objects = NULL;
    freeTable(&vm.strings);
    vm.stackTop = 0;
}


static Chunk *chunk() { return vm.chunk; }
static uint8_t *ip() { return vm.ip; }
static Value *stack() { return vm.stack; }
static int stackTop() { return vm.stackTop; }


static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int line = chunk()->lines[ip() - chunk()->code - 1];

    fprintf(stderr, "runtime error: [at line %d]: ", line);
    vfprintf(stderr, format, args);
    va_end(args);

    longjmp(buf, 1);
}


void push(Value value) {
    if (stackTop() >= 256) {
        runtimeError("stack overflow");
    }
    stack()[vm.stackTop++] = value;
}


Value pop() {
    if (stackTop() == 0) {
        runtimeError("stack underflow");
    }

    Value popped = stack()[stackTop() - 1];
    vm.stackTop--;
    return popped;
}


Value top() {
    if (stackTop() == 0) {
        runtimeError("stack is empty");
    }
    return stack()[stackTop() - 1];
}


static void printAssemblyAndStack(size_t offset) {
    for (int i = 0; i < stackTop(); i++) {
        printf("[");
        printValue(stack()[i]);
        printf("]");
    }
    printf("\n\n");

    disassembleInstruction(offset, chunk());
    printf("\n\n");

}


static void run() {
#define READ_BYTE() \
    (*vm.ip++)


#define READ_CONSTANT() \
    (chunk()->constantPool[READ_BYTE()])


#define READ_OFFSET() \
    (ip() - chunk()->code - 1)


#define PERFORM_ARITHMATIC(OPERATION, operationTag)          \
    do {                                                    \
        OperationResult result = OPERATION(operationTag);   \
        if (result.isError) {                               \
            runtimeError(result.errorMessage);              \
        }                                                   \
    } while (false)


    for (;;) {
        Operation operation = READ_BYTE();


#ifdef PRINT_ASSEMBLY
        printAssemblyAndStack(READ_OFFSET());
#endif


        switch (operation) {
            case OP_CONSTANT: {
                push(READ_CONSTANT());
                break;
            }
            case OP_COMMA: {
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_TERNARY: {
                Value falseValue = pop();
                Value truthValue = pop();
                if (isTruthy(pop())) {
                    push(truthValue);
                } else {
                    push(falseValue);
                }
                break;
            }
            case OP_ADD: {
                PERFORM_ARITHMATIC(performBinary, OP_ADD);
                break;
            }
            case OP_SUB: {
                PERFORM_ARITHMATIC(performBinary, OP_SUB);
                break;
            }
            case OP_MUL: {
                PERFORM_ARITHMATIC(performBinary, OP_MUL);
                break;
            }
            case OP_DIV: {
                PERFORM_ARITHMATIC(performBinary, OP_DIV);
                break;
            }
            case OP_MOD: {
                PERFORM_ARITHMATIC(performBinary, OP_MOD);
                break;
            }
            case OP_POW: {
                PERFORM_ARITHMATIC(performBinary, OP_POW);
                break;
            }
            case OP_AND: {
                performLogical(OP_AND);
                break;
            }
            case OP_OR: {
                performLogical(OP_OR);
                break;
            }
            case OP_EXIT: {
                return;
            }
            default: {
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_OFFSET
}


InterpretResult interpret(const char *source) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = INTERPRET_OK;
    if (setjmp(buf)) {
        result = INTERPRET_RUNTIME_ERROR;
    } else {
        run();
    }

    freeVM();
    return result;
}
