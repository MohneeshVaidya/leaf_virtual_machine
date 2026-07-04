#include <stdio.h>
#include <string.h>
#include <stdint.h>


#include "object.h"
#include "chunk.h"
#include "forward.h"
#include "value.h"
#include "vm.h"
#include "memory.h"


#define ALLOCATE_OBJ(type, typeTag) \
    ((type*)allocateObj(typeTag, sizeof(type)))


static Obj *allocateObj(ObjType type, size_t bytes) {
    Obj *obj = reallocate(NULL, 0, bytes);

    obj->type = type;
    obj->isMarked = false;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}


static uint32_t generateHash(const char *chars, size_t length, uint32_t initialHash) {
    uint32_t hash= initialHash;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint32_t)chars[i];
        hash *= 16777619;
    }
    return hash;
}


ObjString *makeString(const char *chars, size_t length) {
    uint32_t hash = generateHash(chars, length, 2166136261u);

    ObjString *string = tableFindString(&vm.strings, chars, length, hash);
    if (string) {
        return string;
    }

    string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

    string->length = length;
    string->hash = hash;

    string->meta.isMarked = true;

    string->chars = GROW_ARRAY(char, NULL, 0, length + 1);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    tableSet(&vm.strings, string, NIL_VALUE);

    string->meta.isMarked = false;
    return string;
}


ObjString *addStrings(ObjString *a, ObjString *b) {
    size_t length = a->length + b->length;

    char chars[length + 1];
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    return makeString(chars, length);
}


ObjFunction *makeFunction() {
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);

    function->name = NULL;
    function->arity = 0;
    initChunk(&function->chunk);
    for (int i = 0; i < 256; i++) {
        function->upvalues[i].name = NULL;
        function->upvalues[i].value = NIL_VALUE;
    }
    function->upvaluesFilled = false;
    return function;
}


void printObject(Obj *obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString*)obj;
            printf("%s", string->chars);
            return;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction*)obj;
            printf("<function %s - %d", function->name->chars, function->arity);
            if (function->arity > 0) {
                printf(" - ");
            }
            for (int i = 0; i < function->arity; i++) {
                printObject((Obj*)function->parameters[i]);
                printf(", ");
            }
            if (function->arity == 0) {
                printf(">");
            } else {
                printf("\b\b>");
            }
            return;
        }
    }
}
