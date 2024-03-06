// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// John FrostFox (john.frostfox@gmail.com)
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hashtable.c
/// \brief Hashtable abstraction implementation

#include "hashtable.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HT_NUMLISTS UINT8_MAX

typedef struct mobjnum_linkedList_s{
    thinker_t* thinker;
    struct mobjnum_linkedList_s* prev; //the first element must always be null
    struct mobjnum_linkedList_s* next; //the last element must always be null
}
mobjnum_linkedList;

mobjnum_linkedList mobjnum_Hashtable[HT_NUMLISTS]; //dumb and idiotic, there are up to UINT8_MAX linked lists.
//Wish I could just simply make a mobj_t array with UINT64_MAX entries, LOL

void mobjnum_ht_linkedList_Wipe();
/** Initializes simple mobjnum/thinker hashtable linked list.*/
void mobjnum_ht_linkedList_Init()
{
    mobjnum_ht_linkedList_Wipe();
    for (uint8_t i = 0; i < HT_NUMLISTS; i++)
    {
        mobjnum_Hashtable[i].thinker = NULL;
        mobjnum_Hashtable[i].prev = NULL;
        mobjnum_Hashtable[i].next = NULL;
    }
    
}

/** Adds a thinker's address, determines in which list to add automatically
  *
  * \param mobj     Which mobj's memory address to add 
  */
void mobjnum_ht_linkedList_AddEntry (thinker_t* thinker)
{

    mobjnum_linkedList* currentEntry; // = &mobjnum_Hashtable[(UINT8)(mobj->mobjnum % HT_NUMLISTS)];
    mobjnum_linkedList* next;

    currentEntry = &mobjnum_Hashtable[(UINT8)(((mobj_t*)thinker)->mobjnum % HT_NUMLISTS)];

    if (!currentEntry->next) // check for the first entry
    {
        currentEntry->thinker = thinker;
        // CONS_Printf("next not exists, create\n");
        currentEntry->next = malloc(sizeof(mobjnum_linkedList));
        currentEntry->next->thinker = NULL;
        currentEntry->next->next = NULL;
        currentEntry->next->prev = currentEntry;
        return;
    }
    
    for (currentEntry = &mobjnum_Hashtable[(UINT8)(((mobj_t*)thinker)->mobjnum % HT_NUMLISTS)]; currentEntry != NULL; currentEntry = next)
    {
        if (!currentEntry->thinker)
        {
            currentEntry->thinker = thinker;
            break;
        }
        else
        {
            if (!currentEntry->next)
            {
                currentEntry->next = malloc(sizeof(mobjnum_linkedList));
                currentEntry->next->thinker = thinker;
                currentEntry->next->next = NULL;
                currentEntry->next->prev = currentEntry;
                next = NULL;
            }
            else
                next = currentEntry->next;
        }
    }
}

/** Removes a thinker's address by its mobjnum, determines from which list to remove automatically
  *
  * \param mobjnum     Which mobj's memory address to remove by itsmobjnum 
  */
// void mobjnum_ht_linkedList_RemoveEntry (uint8_t mobjnumber)
// {
//     mobjnum_linkedList* currentEntry = &mobjnum_Hashtable[(UINT8)(mobjnumber % HT_NUMLISTS)];
//     if (currentEntry->next == &currentEntry)
//         currentEntry->mobj = &mobj;
//     else
//     {
//         currentEntry->next = malloc(sizeof(mobjnum_linkedList));

//     }
// }

/** Looks for a thinker by its mobjnum.
  *
  * \param mobjnumber mobj's mobjnum
  * \return thinker_t* if an object is found, otherwise NULL. Cast it to mobj_t* to get the mobj.
  */
thinker_t* mobjnum_ht_linkedList_Find (uint32_t mobjnumber)
{
    // mobjnum_linkedList* currentEntry = &mobjnum_Hashtable[(UINT8)(mobjnumber % HT_NUMLISTS)];

    mobjnum_linkedList* currentEntry; // = &mobjnum_Hashtable[(UINT8)(mobj->mobjnum % HT_NUMLISTS)];
    mobjnum_linkedList* next;

    if (!mobjnumber)
        return NULL;

    currentEntry = &mobjnum_Hashtable[(UINT8)(mobjnumber % HT_NUMLISTS)];

    if (!currentEntry->next) // check for the first entry
    {
        if (!currentEntry->thinker)
            return NULL;

        if (((mobj_t *)currentEntry->thinker)->mobjnum == mobjnumber)
            return currentEntry->thinker;
    }

    for (currentEntry = &mobjnum_Hashtable[(UINT8)(mobjnumber % HT_NUMLISTS)]; currentEntry != NULL; currentEntry = next)
    {
        if (!currentEntry->thinker)
            return NULL;

        if (((mobj_t *)currentEntry->thinker)->mobjnum == mobjnumber)
            return currentEntry->thinker;
        if (currentEntry->next)
            next = currentEntry->next;
        else
            next = NULL;
    }
    return NULL;
}

/** Wipes the entire list by iterating everything it has*/
void mobjnum_ht_linkedList_Wipe()
{
    mobjnum_linkedList* currentEntry; // = &mobjnum_Hashtable[(UINT8)(mobj->mobjnum % HT_NUMLISTS)];
    mobjnum_linkedList* next;

    for (UINT8 i = 0; i < HT_NUMLISTS; i++)
    {
        if (!mobjnum_Hashtable[i].next)
            continue;
        for (currentEntry = &mobjnum_Hashtable[i]; currentEntry != NULL; currentEntry = next)
        {
            // bye!
            if (currentEntry->next)
                next = currentEntry->next;
            else
                next = NULL;
            if (currentEntry->next)
            {
                currentEntry->next = NULL;
            }
            if (currentEntry->thinker)
            {
                currentEntry->thinker = NULL;
            }
            if (currentEntry->prev) //prevents freeing the first element
            {
                currentEntry->prev = NULL;
                free(currentEntry);
            }
        }
    }
    return;
}

// Simple hash table implemented in C.

// Hash table entry (slot may be filled or empty).
typedef struct {
    const char* key;  // key is NULL if this slot is empty
    void* value;
} ht_entry;

// Hash table structure: create with ht_create, free with ht_destroy.
struct hashtable {
    ht_entry* entries;  // hash slots
    size_t capacity;    // size of _entries array
    size_t length;      // number of items in hash table
};

#define INITIAL_CAPACITY 16  // must not be zero

hashtable* hashtable_Create(void) {
    // Allocate space for hash table struct.
    hashtable* table = malloc(sizeof(hashtable));
    if (table == NULL) {
        return NULL;
    }
    table->length = 0;
    table->capacity = INITIAL_CAPACITY;

    // Allocate (zero'd) space for entry buckets.
    table->entries = calloc(table->capacity, sizeof(ht_entry));
    if (table->entries == NULL) {
        free(table); // error, free table before we return!
        return NULL;
    }
    return table;
}

void hashtable_Destroy(hashtable* table) {
    // First free allocated keys.
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->entries[i].key != NULL) {
            free((void*)table->entries[i].key);
        }
    }

    // Then free entries array and table itself.
    free(table->entries);
    free(table);
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char* key) {
    uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

void* hashtable_Get(hashtable* table, const UINT32* key) {
    // AND hash with capacity-1 to ensure it's within entries array.
    uint64_t hash = hash_key(key);
    size_t index = (size_t)(hash & (uint64_t)(table->capacity - 1));

    // Loop till we find an empty entry.
    while (table->entries[index].key != NULL) {
        if (strcmp(key, table->entries[index].key) == 0) {
            // Found key, return value.
            return table->entries[index].value;
        }
        // Key wasn't in this slot, move to next (linear probing).
        index++;
        if (index >= table->capacity) {
            // At end of entries array, wrap around.
            index = 0;
        }
    }
    return NULL;
}

// Internal function to set an entry (without expanding table).
static const char* ht_set_entry(ht_entry* entries, size_t capacity,
        const char* key, void* value, size_t* plength) {
    // AND hash with capacity-1 to ensure it's within entries array.
    uint64_t hash = hash_key(key);
    size_t index = (size_t)(hash & (uint64_t)(capacity - 1));

    // Loop till we find an empty entry.
    while (entries[index].key != NULL) {
        if (strcmp(key, entries[index].key) == 0) {
            // Found key (it already exists), update value.
            entries[index].value = value;
            return entries[index].key;
        }
        // Key wasn't in this slot, move to next (linear probing).
        index++;
        if (index >= capacity) {
            // At end of entries array, wrap around.
            index = 0;
        }
    }

    // Didn't find key, allocate+copy if needed, then insert it.
    if (plength != NULL) {
        key = strdup(key);
        if (key == NULL) {
            return NULL;
        }
        (*plength)++;
    }
    entries[index].key = (char*)key;
    entries[index].value = value;
    return key;
}

// Expand hash table to twice its current size. Return true on success,
// false if out of memory.
static boolean ht_expand(hashtable* table) {
    // Allocate new entries array.
    size_t new_capacity = table->capacity * 2;
    if (new_capacity < table->capacity) {
        return false;  // overflow (capacity would be too big)
    }
    ht_entry* new_entries = calloc(new_capacity, sizeof(ht_entry));
    if (new_entries == NULL) {
        return false;
    }

    // Iterate entries, move all non-empty ones to new table's entries.
    for (size_t i = 0; i < table->capacity; i++) {
        ht_entry entry = table->entries[i];
        if (entry.key != NULL) {
            ht_set_entry(new_entries, new_capacity, entry.key,
                         entry.value, NULL);
        }
    }

    // Free old entries array and update this table's details.
    free(table->entries);
    table->entries = new_entries;
    table->capacity = new_capacity;
    return true;
}

const char* hashtable_Set(hashtable* table, const UINT32* key, void* value) {
    assert(value != NULL);
    if (value == NULL) {
        return NULL;
    }

    // If length will exceed half of current capacity, expand it.
    if (table->length >= table->capacity / 2) {
        if (!ht_expand(table)) {
            return NULL;
        }
    }

    // Set entry and update length.
    return ht_set_entry(table->entries, table->capacity, key, value,
                        &table->length);
}

size_t hashtable_Length(hashtable* table) {
    return table->length;
}

hashtable_iterator ht_iterator(hashtable* table) {
    hashtable_iterator it;
    it._table = table;
    it._index = 0;
    return it;
}

boolean hashtable_Next(hashtable_iterator* it) {
    // Loop till we've hit end of entries array.
    hashtable* table = it->_table;
    while (it->_index < table->capacity) {
        size_t i = it->_index;
        it->_index++;
        if (table->entries[i].key != NULL) {
            // Found next non-empty item, update iterator key and value.
            ht_entry entry = table->entries[i];
            it->key = entry.key;
            it->value = entry.value;
            return true;
        }
    }
    return false;
}