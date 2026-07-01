#include "table.h"
#include "memory.h"
#include "value.h"
#include <string.h>


#define INITIAL_CAPACITY (8)
#define LOAD_FACTOR      (0.75)


static void initEntries(Entry *entries, size_t capacity) {
    for (size_t i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VALUE;
    }
}


void initTable(Table *table) {
    table->capacity = INITIAL_CAPACITY;
    table->count = 0;
    table->entries = GROW_ARRAY(Entry, NULL, 0, table->capacity);
    initEntries(table->entries, table->capacity);
}


void freeTable(Table *table) {
    FREE(Entry, table->entries, table->capacity);
    initTable(table);
}


static bool isEmpty(Entry *entry) {
    return entry->key == NULL && IS_NIL(entry->value);
}


static bool isTombstone(Entry *entry) {
    return entry->key == NULL && IS_TOMBSTONE(entry->value);
}


static Entry *findEntryForInsertion(Entry *entries, size_t capacity, ObjString *key) {
    size_t index = key->hash % capacity;

    Entry *tombstone = NULL;
    for (size_t i = 0; i < capacity; i++) {
        Entry *entry = entries + index;

        if (!tombstone && isTombstone(entry)) {
            tombstone = entry;
        }
        else if (isEmpty(entry)) {
            return tombstone ? tombstone : entry;
        }
        else if (key == entry->key) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
    return NULL;
}


static void ensureCapacity(Table *table) {
    if (table->capacity * LOAD_FACTOR > table->count) {
        return;
    }

    size_t capacity = GROW_CAPACITY(table->capacity);
    size_t count = 0;
    Entry *entries = GROW_ARRAY(Entry, NULL, 0, capacity);

    initEntries(entries, capacity);

    for (size_t i = 0; i < table->capacity; i++) {
        Entry *current = table->entries + i;
        if (current->key == NULL) {
            continue;
        }
        Entry *entry = findEntryForInsertion(entries, capacity, current->key);
        entry->key = current->key;
        entry->value = current->value;
        count++;
    }

    FREE(Entry, table->entries, table->capacity);

    table->capacity = capacity;
    table->count = count;
    table->entries = entries;
}


bool tableSet(Table *table, ObjString *key, Value value) {
    ensureCapacity(table);

    Entry *entry = findEntryForInsertion(table->entries, table->capacity, key);
    if (!entry) {
        return false;
    }
    if (isEmpty(entry)) {
        table->count++;
    }
    entry->key = key;
    entry->value = value;
    return true;
}


static Entry *findEntryForLookup(Entry *entries, size_t capacity, ObjString *key) {
    size_t index = key->hash % capacity;

    for (size_t i = 0; i < capacity; i++) {
        Entry *entry = entries + index;

        if (isEmpty(entry)) {
            break;
        }

        if (key == entry->key) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
    return NULL;
}


bool tableGet(Table *table, ObjString *key, Value *value) {
    if (table->count == 0) return false;

    Entry *entry = findEntryForLookup(table->entries, table->capacity, key);
    if (!entry) {
        return false;
    }

    if (value) {
        *value = entry->value;
    }

    return true;
}


bool tableRemove(Table *table, ObjString *key) {
    if (table->count == 0) return false;

    Entry *entry = findEntryForLookup(table->entries, table->capacity, key);
    if (!entry) {
        return false;
    }

    entry->key = NULL;
    entry->value = TOMBSTONE_VALUE;

    return true;
}


bool tableContains(Table *table, ObjString *key) {
    return tableGet(table, key, NULL);
}


ObjString *tableFindString(Table *table, const char *chars, size_t length, uint32_t hash) {
    size_t index = hash % table->capacity;

    for (size_t i = 0; i < table->capacity; i++) {
        Entry *entry = table->entries + index;

        if (isEmpty(entry)) {
            break;
        }

        if (
            entry->key &&
            (hash == entry->key->hash) &&
            (length == entry->key->length) &&
            (memcmp(chars, entry->key->chars, length) == 0)
        ) {
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }

    return NULL;
}
