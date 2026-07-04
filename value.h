#ifndef leaf_value_h
#define leaf_value_h


#include "forward.h"


#define NUMBER_VALUE(value)     ((Value){ VALUE_NUMBER, { .number = value } })
#define BOOLEAN_VALUE(value)    ((Value){ VALUE_BOOLEAN, { .boolean = value } })
#define NIL_VALUE               ((Value){ VALUE_NIL, { .number = 0 } })
#define TOMBSTONE_VALUE         ((Value){ VALUE_TOMBSTONE, { .number = 0 } })
#define OBJ_VALUE(value)        ((Value){ VALUE_OBJ, { .obj = (Obj*)value } })


#define IS_NUMBER(value)        ((value).type == VALUE_NUMBER)
#define IS_BOOLEAN(value)       ((value).type == VALUE_BOOLEAN)
#define IS_NIL(value)           ((value).type == VALUE_NIL)
#define IS_TOMBSTONE(value)     ((value).type == VALUE_TOMBSTONE)
#define IS_OBJ(value)           ((value).type == VALUE_OBJ)
#define IS_STRING(value)        (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value)      (isObjType(value, OBJ_FUNCTION))


#define AS_NUMBER(value)        ((value).as.number)
#define AS_BOOLEAN(value)       ((value).as.boolean)
#define AS_OBJ(value)           ((value).as.obj)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))


void printValue(Value value);
bool isTruthy(Value value);
bool isCallable(Value value);


inline static bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && (AS_OBJ(value)->type == type);
}


#endif
