#include <math.h>


#include "operation.h"
#include "chunk.h"
#include "value.h"
#include "vm.h"


OperationResult performBinary(Operation operation) {
    OperationResult result = { false, "" };

    Value b = pop();
    Value a = pop();


#define CHECK_TYPE(IS_TYPE) \
    (IS_TYPE(a) && IS_TYPE(b))


#define BINARY_OPERATION(op, symbol)                                                \
    do {                                                                            \
        if (!CHECK_TYPE(IS_NUMBER)) {                                               \
            result.isError = true;                                                  \
            result.errorMessage = "'"symbol"' expects both operands to be number";  \
        } else {                                                                    \
            push(NUMBER_VALUE(AS_NUMBER(a) op AS_NUMBER(b)));                       \
        }                                                                           \
    } while (false)


    switch (operation) {
        case OP_ADD: {
            if (!CHECK_TYPE(IS_NUMBER) && !CHECK_TYPE(IS_STRING)) {
                result.isError = true;
                result.errorMessage = "'+' expects both operands to be either number or string";
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
                result.isError = true;
                result.errorMessage = "'%' expects both operands to be number";
            } else {
                push(NUMBER_VALUE(fmod(AS_NUMBER(a), AS_NUMBER(b))));
            }
            break;
        }
        case OP_POW: {
            if (!CHECK_TYPE(IS_NUMBER)) {
                result.isError = true;
                result.errorMessage = "'**' expects both operands to be number";
            } else {
                push(NUMBER_VALUE(pow(AS_NUMBER(a), AS_NUMBER(b))));
            }
            break;
        }
        default:
            break;
    }

    return result;

#undef CHECK_TYPE
#undef BINARY_OPERATION
}



OperationResult performLogical(Operation operation) {
    OperationResult result = { false, "" };

    Value b = pop();
    Value a = pop();

    switch (operation) {
        case OP_AND: {
            if (!isTruthy(a)) {
                push(a);
            } else {
                push(b);
            }
            break;
        }
        case OP_OR: {
            if (isTruthy(a)) {
                push(a);
            } else {
                push(b);
            }
            break;
        }
        default: break;
    }

    return result;
}
