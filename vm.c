#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>


#include "vm.h"
#include "compiler.h"
#include "forward.h"
#include "operation.h"
#include "table.h"
#include "value.h"
#include "disassembler.h"


jmp_buf buf;


VM vm;


static void initVM() {
    // vm.chunk = NULL;
    // vm.ip = NULL;

    vm.objects = NULL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.stackTop = 0;
    vm.framesTop = 0;
}


static void freeVM() {
    // vm.chunk = NULL;
    // vm.ip = NULL;

    vm.objects = NULL;

    freeTable(&vm.globals);
    freeTable(&vm.strings);

    vm.stackTop = 0;
    vm.framesTop = 0;
}


static void runtimeError(const char *format, ...);


static int framesTop() {
    return vm.framesTop;
}


static CallFrame *topFrame() {
    return &vm.frames[framesTop()-1];
}


static void pushFrame(ObjFunction *function, Value *stack) {
    if (framesTop() >= 10240) {
        runtimeError("frame_stack overflow");
    }
    vm.frames[framesTop()].function = function;
    vm.frames[framesTop()].frameStack = stack;
    vm.frames[framesTop()].ip = function->chunk.code;
    vm.framesTop++;
}

static void popFrame() {
    if (framesTop() == 0) {
        runtimeError("frame_stack underflow");
    }
    vm.framesTop--;
}


static Chunk *chunk() { return &topFrame()->function->chunk; }
static uint8_t *ip() { return topFrame()->ip; }
static Value *frameStack() { return topFrame()->frameStack; }

static Table *globals() { return &vm.globals; }
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


Value peek(int index) {
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

    disassembleInstruction(offset, topFrame()->function);
    printf("\n");

}


static void run() {
#define READ_BYTE() \
    (*topFrame()->ip++)


#define READ_CONSTANT() \
    (chunk()->constantPool[READ_BYTE()])


#define READ_OFFSET() \
    (ip() - chunk()->code - 1)


#define READ_JUMP_TARGET() \
    (chunk()->code + (int)(READ_BYTE() << 8 | READ_BYTE()))


#define READ_NAME() \
    (AS_STRING(READ_CONSTANT()))


#define PERFORM_ARITHMATIC(OPERATION, operationTag)          \
    do {                                                    \
        OperationResult result = OPERATION(operationTag);   \
        if (result.isError) {                               \
            runtimeError(result.errorMessage);              \
        }                                                   \
    } while (false)


    for (;;) {
        Operation operation = READ_BYTE();


#ifdef PRINT_PER_INSTRUNCTION_ASSEMBLY
        printAssemblyAndStack(READ_OFFSET());
#endif


        switch (operation) {
            case OP_CONSTANT: {
                push(READ_CONSTANT());
                break;
            }
            case OP_POP: {
                pop();
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
                    topFrame()->ip = READ_JUMP_TARGET();
                } else {
                    topFrame()->ip += 2;
                }
                break;
            }
            case OP_JUMP: {
                topFrame()->ip = READ_JUMP_TARGET();
                break;
            }

            case OP_DECLARE_GLOBAL: {
                ObjString *name = READ_NAME();
                tableSet(globals(), name, top());
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_NAME();
                Value value;
                if (!tableGet(globals(), name, &value)) {
                    runtimeError("accessing undeclared name '%s'", name->chars);
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_NAME();
                if (!tableContains(globals(), name)) {
                    runtimeError("accessing undeclared name '%s'", name->chars);
                }
                tableSet(globals(), name, top());
                break;
            }

            case OP_DECLARE_LOCAL: {
                READ_NAME();
                break;
            }
            case OP_GET_LOCAL: {
                int index = READ_BYTE();
                push(frameStack()[index]);
                break;
            }
            case OP_SET_LOCAL: {
                int index = READ_BYTE();
                frameStack()[index] = top();
                if (IS_FUNCTION(top())) {
                    ObjFunction *function = AS_FUNCTION(top());
                    if (!function->upvaluesFilled) {
                        function->upvaluesFilled = true;
                        for (int i = 0; i < 256; i++) {
                            if (function->upvalues[i].name) {
                                function->upvalues[i].value = peek(i);
                            }
                        }
                    }
                }
                break;
            }

            case OP_GET_UPVALUE: {
                int index = READ_BYTE();
                push(topFrame()->function->upvalues[index].value);
                break;
            }
            case OP_SET_UPVALUE: {
                int index = READ_BYTE();
                topFrame()->function->upvalues[index].value = top();
                break;
            }

            case OP_CALL: {
                int argumentCount = READ_BYTE();
                Value object = peek(argumentCount);
                if (IS_FUNCTION(object)) {
                    ObjFunction *function = AS_FUNCTION(object);
                    if (function->arity != argumentCount) {
                        runtimeError("argument count doesn't match the parameter count");
                    }
                    pushFrame(function, (stack() + stackTop() - argumentCount));
                    break;
                }
                runtimeError("called expression is not callable");
                break;
            }

            case OP_RETURN: {
                Value returnValue = pop();
                *(frameStack() - 1) = returnValue;
                vm.stackTop = frameStack() - stack();
                popFrame();
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
#undef READ_NAME
#undef PERFORM_ARITHMATIC
}


InterpretResult interpret(const char *source) {
    initVM();

    ObjFunction *function = compile(source);
    if (!function) {
        return INTERPRET_COMPILE_ERROR;
    }

    pushFrame(function, vm.stack);

#ifdef PRINT_ASSEMBLY
    disassemble(function);
#endif


    InterpretResult result = INTERPRET_OK;
    if (setjmp(buf)) {
        result = INTERPRET_RUNTIME_ERROR;
    } else {
        run();
    }

    freeVM();
    return result;
}
