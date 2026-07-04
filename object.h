#ifndef leaf_object_h
#define leaf_object_h


#include <stdint.h>
#include <stdbool.h>


#include "forward.h"


ObjString *makeString(const char *chars, size_t length);
ObjString *addStrings(ObjString *a, ObjString *b);
ObjFunction *makeFunction();
ObjClosure *makeClosure();
ObjClosure *cloneClosure(ObjClosure *closure);
void printObject(Obj *obj);


#endif
