#include <stdio.h>


#include "value.h"
#include "object.h"


void printValue(Value value) {
    switch (value.type) {
        case VALUE_NUMBER: {
            printf("%.2lf", AS_NUMBER(value));
            return;
        }
        case VALUE_BOOLEAN: {
            printf("%s", AS_BOOLEAN(value) ? "true" : "false");
            return;
        }
        case VALUE_TOMBSTONE: {
            printf("R.I.P");
            return;
        }
        case VALUE_NIL: {
            printf("nil");
            return;
        }
        case VALUE_OBJ: {
            printObject(AS_OBJ(value));
            return;
        }
        default: {
            printf("default value");
            return;
        }
    }
}


bool isTruthy(Value value) {
    switch (value.type) {
        case VALUE_NUMBER: {
            return !!AS_NUMBER(value);
        }
        case VALUE_BOOLEAN: {
            return AS_BOOLEAN(value);
        }
        case VALUE_NIL:
        case VALUE_TOMBSTONE: {
            return false;
        }
        case VALUE_OBJ: {
            if (IS_STRING(value)) return !!AS_STRING(value)->length;
            return true;
        }
        default:
            break;
    }
    return true;
}


bool isCallable(Value value) {
    return IS_FUNCTION(value);
}
