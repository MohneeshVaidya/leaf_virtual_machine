#ifndef leaf_table_h
#define leaf_table_h


#include <stdbool.h>


#include "forward.h"


void initTable(Table *table);
void freeTable(Table *table);
bool tableSet(Table *table, ObjString *key, Value value);
bool tableGet(Table *table, ObjString *key, Value *value);
bool tableRemove(Table *table, ObjString *key);
bool tableContains(Table *table, ObjString *key);
ObjString *tableFindString(Table *table, const char *chars, size_t length, uint32_t hash);


#endif
