#include <math.h>


#include "operation.h"
#include "chunk.h"
#include "value.h"
#include "vm.h"


#define CHECK_TYPE(IS_TYPE) \
    (IS_TYPE(a) && IS_TYPE(b))


static void setError(OperationResult *result, const char *errorMessage) {
    result->isError = true;
    result->errorMessage = errorMessage;
}


OperationResult performBinary(Operation operation) {
    OperationResult result = { false, "" };

    Value b = pop();
    Value a = pop();


#define BINARY_OPERATION(op, symbol)                                                \
    do {                                                                            \
        if (!CHECK_TYPE(IS_NUMBER)) {                                               \
            setError(&result, "'"symbol"' expects both operands to be numbers");    \
        } else {                                                                    \
            push(NUMBER_VALUE(AS_NUMBER(a) op AS_NUMBER(b)));                       \
        }                                                                           \
    } while (false)


    switch (operation) {
        case OP_ADD: {
            if (!CHECK_TYPE(IS_NUMBER) && !CHECK_TYPE(IS_STRING)) {
                setError(&result, "'+' expects both operands to be either numbers or strings");
            } else if (CHECK_TYPE(IS_NUMBER)) {
                push(NUMBER_VALUE(AS_NUMBER(a) + AS_NUMBER(b)));
            } else {
                push(OBJ_VALUE(addStrings(AS_STRING(a), AS_STRING(b))));
            }
            break;
        }
        case OP_SUB: {
            BINARY_OPERATION(-, "-");
            break;
        }
        case OP_MUL: {
            BINARY_OPERATION(*, "*");
            break;
        }
        case OP_DIV: {
            BINARY_OPERATION(/, "/");
            break;
        }
        case OP_MOD: {
            if (!CHECK_TYPE(IS_NUMBER)) {
                setError(&result, "'%' expects both operands to be numbers");
            } else {
                push(NUMBER_VALUE(fmod(AS_NUMBER(a), AS_NUMBER(b))));
            }
            break;
        }
        case OP_POW: {
            if (!CHECK_TYPE(IS_NUMBER)) {
                setError(&result, "'**' expects both operands to be numbers");
            } else {
                push(NUMBER_VALUE(pow(AS_NUMBER(a), AS_NUMBER(b))));
            }
            break;
        }
        default:
            break;
    }

    return result;

#undef BINARY_OPERATION
}


OperationResult performUnary(Operation operation) {
    OperationResult result = { false, "" };

    Value value = pop();

    switch (operation) {
        case OP_NEGATE: {
            if (!IS_NUMBER(value)) {
                setError(&result, "unary '+' expects only number as operand");
            }
            push(NUMBER_VALUE(-AS_NUMBER(value)));
            break;
        }
        case OP_NOT: {
            push(BOOLEAN_VALUE(!isTruthy(value)));
            break;
        }
        case OP_POS: {
            push(value);
            break;
        }
        default: break;
    }
    return result;
}


OperationResult performComparision(Operation operation) {
    OperationResult result = { false, "" };

    Value b = pop();
    Value a = pop();

    if (a.type != b.type) {
        setError(&result, "equality and comparision operators expect both operands to be of same type");
        return result;
    }


#define PERFORM_EQUALITY(op)                                            \
    do {                                                                \
        if (CHECK_TYPE(IS_NUMBER)) {                                    \
            push(BOOLEAN_VALUE(AS_NUMBER(a) op AS_NUMBER(b)));          \
        } else if (CHECK_TYPE(IS_BOOLEAN)) {                            \
            push(BOOLEAN_VALUE(AS_BOOLEAN(a) op AS_BOOLEAN(b)));        \
        } else if (CHECK_TYPE(IS_NIL) || CHECK_TYPE(IS_TOMBSTONE)) {    \
            push(BOOLEAN_VALUE(true));                                  \
        } else {                                                        \
            push(BOOLEAN_VALUE(AS_OBJ(a) op AS_OBJ(b)));                \
        }                                                               \
    } while (false)


#define PERFORM_COMPARISON(op)                                                              \
    do {                                                                                    \
        if (!CHECK_TYPE(IS_NUMBER)) {                                                       \
            setError(&result, "comparision operators expect both operands to be numbers");  \
        } else {                                                                            \
            push(BOOLEAN_VALUE(AS_NUMBER(a) op AS_NUMBER(b)));                              \
        }                                                                                   \
    } while (false)


    switch (operation) {
        case OP_EQUAL: {
            PERFORM_EQUALITY(==);
            break;
        }
        case OP_NOT_EQUAL: {
            PERFORM_EQUALITY(!=);
            break;
        }
        case OP_LESSER: {
            PERFORM_COMPARISON(<);
            break;
        }
        case OP_LESSER_EQUAL: {
            PERFORM_COMPARISON(<=);
            break;
        }
        case OP_GREATER: {
            PERFORM_COMPARISON(>);
            break;
        }
        case OP_GREATER_EQUAL: {
            PERFORM_COMPARISON(>=);
            break;
        }
        default: break;
    }

    return result;

#undef PERFORM_EQUALITY
#undef PERFORM_COMPARISON
}
