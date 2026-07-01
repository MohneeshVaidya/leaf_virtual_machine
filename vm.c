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

    vm.scope.scopeDepth = 0;

    vm.objects = NULL;
    initTable(&vm.strings);

    vm.stackTop = 0;
}


static void freeVM() {
    vm.chunk = NULL;
    vm.ip = NULL;

    vm.scope.scopeDepth = 0;

    vm.objects = NULL;
    freeTable(&vm.strings);

    vm.stackTop = 0;
}


static Chunk *chunk() { return vm.chunk; }
static uint8_t *ip() { return vm.ip; }
static Scope *scope() { return &vm.scope; }
static Value *stack() { return vm.stack; }
static int stackTop() { return vm.stackTop; }


static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int line = chunk()->lines[ip() - chunk()->code - 1];

    fprintf(stderr, "runtime error: [at line %d]: ", line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
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


static Value peek(int index) {
    int stackIndex = stackTop() - index - 1;
    if (stackIndex < 0) {
        runtimeError("can not peek stack at index %d", stackIndex);
    }
    return stack()[stackIndex];
}


Value top() {
    return peek(0);
}


static void printAssemblyAndStack(size_t offset) {
    if (stackTop() == 0) {
        printf("[]");
    }
    for (int i = 0; i < stackTop(); i++) {
        printf("[");
        printValue(stack()[i]);
        printf("]");
    }
    printf("\n");

    disassembleInstruction(offset, chunk());
    printf("\n");

}


static void run() {
#define READ_BYTE() \
    (*vm.ip++)


#define READ_CONSTANT() \
    (chunk()->constantPool[READ_BYTE()])


#define READ_OFFSET() \
    (ip() - chunk()->code - 1)


#define READ_JUMP_TARGET() \
    (chunk()->code + (int)(READ_BYTE() << 8 | READ_BYTE()))


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
            case OP_POP: {
                (void)pop();
                break;
            }
            case OP_COMMA: {
                Value value = pop();
                pop();
                push(value);
                break;
            }

            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
            case OP_POW: {
                PERFORM_ARITHMATIC(performBinary, operation);
                break;
            }

            case OP_NOT:
            case OP_POS:
            case OP_NEGATE: {
                performUnary(operation);
                break;
            }
            case OP_EQUAL:
            case OP_NOT_EQUAL:
            case OP_LESSER:
            case OP_LESSER_EQUAL:
            case OP_GREATER:
            case OP_GREATER_EQUAL:
                performComparision(operation);
                break;

            case OP_PRINT: {
                printValue(pop());
                break;
            }
            case OP_PRINTLN: {
                printValue(pop());
                printf("\n");
                break;
            }

            case OP_JUMP_IF_FALSE: {
                if (!isTruthy(top())) {
                    vm.ip = READ_JUMP_TARGET();
                } else {
                    vm.ip += 2;
                }
                break;
            }

            case OP_JUMP: {
                vm.ip = READ_JUMP_TARGET();
                break;
            }
            case OP_EXIT: {
                return;
            }
            default:
                break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_OFFSET
#undef READ_JUMP_TARGET
#undef PERFORM_ARITHMATIC
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
