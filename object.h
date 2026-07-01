#ifndef leaf_object_h
#define leaf_object_h


#include <stdint.h>
#include <stdbool.h>


typedef enum ObjType {
    OBJ_STRING,
} ObjType;


typedef struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj *next;
} Obj;


typedef struct ObjString {
    Obj meta;
    char *chars;
    size_t length;
    uint32_t hash;
} ObjString;


ObjString *makeString(const char *chars, size_t length);
ObjString *addStrings(ObjString *a, ObjString *b);
void printObject(Obj *obj);


#endif
