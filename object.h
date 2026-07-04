#ifndef leaf_object_h
#define leaf_object_h


#include <stdint.h>
#include <stdbool.h>


#include "forward.h"


ObjString *makeString(const char *chars, size_t length);
ObjString *addStrings(ObjString *a, ObjString *b);
ObjFunction *makeFunction();
void printObject(Obj *obj);


#endif
