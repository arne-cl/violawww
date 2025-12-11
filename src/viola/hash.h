#pragma once
/* hash.h
 *
 */
#include <stdint.h>

typedef struct HashEntry {
    struct HashEntry* next;
    intptr_t label;
    intptr_t val;
} HashEntry;

typedef struct HashTable {
    HashEntry* entries;
    int size;
    int (*func_hash)(struct HashTable*, intptr_t);
    intptr_t (*func_cmp)(intptr_t, intptr_t);
    void (*func_freeLabel)(intptr_t);
    void (*func_freeVal)(intptr_t);
    HashEntry* (*get)(struct HashTable*, intptr_t);
    HashEntry* (*put)(struct HashTable*, intptr_t, intptr_t);
    HashEntry* (*put_replace)(struct HashTable*, intptr_t, intptr_t);
    int (*remove)(struct HashTable*, intptr_t);
} HashTable;

int hash_int(HashTable*, intptr_t);
int hash_str(HashTable*, intptr_t);
HashTable* initHashTable(int size, 
                         int (*func_hash)(HashTable*, intptr_t), 
                         intptr_t (*func_cmp)(intptr_t, intptr_t), 
                         void (*func_freeLabel)(intptr_t), 
                         void (*func_freeVal)(intptr_t), 
                         HashEntry* (*func_get)(HashTable*, intptr_t), 
                         HashEntry* (*func_put)(HashTable*, intptr_t, intptr_t), 
                         HashEntry* (*func_put_replace)(HashTable*, intptr_t, intptr_t), 
                         int (*func_remove)(HashTable*, intptr_t));

HashEntry* putHashEntry(struct HashTable*, intptr_t, intptr_t);
HashEntry* putHashEntry_replace(struct HashTable*, intptr_t, intptr_t);
HashEntry* getHashEntry(struct HashTable*, intptr_t);
int removeHashEntry(struct HashTable*, intptr_t);

HashEntry* putHashEntry_cancelable_int(HashTable* ht, intptr_t label, intptr_t val);

HashEntry* putHashEntry_int(HashTable* ht, intptr_t label, intptr_t val);
HashEntry* putHashEntry_replace_int(HashTable* ht, intptr_t label, intptr_t val);
HashEntry* getHashEntry_int(HashTable* ht, intptr_t label);
int removeHashEntry_int(HashTable* ht, intptr_t label);

HashEntry* putHashEntry_str(HashTable* ht, intptr_t label, intptr_t val);
HashEntry* putHashEntry_replace_str(HashTable* ht, intptr_t label, intptr_t val);
HashEntry* getHashEntry_str(HashTable* ht, intptr_t label);
int removeHashEntry_str(HashTable* ht, intptr_t label);

void dumpHashTable(HashTable*);
