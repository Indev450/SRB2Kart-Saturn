// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// John FrostFox (john.frostfox@gmail.com)
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hashtable.h
/// \brief Hashtable abstraction

#include "doomdef.h"
#include "p_mobj.h"
#include "d_think.h"

// extern uint16_t	 hashHits;
// extern uint16_t	 hashMiss;

void mobjnum_ht_linkedList_Init();
void mobjnum_ht_linkedList_AddEntry (thinker_t* thinker);
thinker_t* mobjnum_ht_linkedList_Find (uint32_t mobjnumber);
// void mobjnum_ht_linkedList_Wipe();

// Hash table iterator: create with ht_iterator, iterate with ht_next.
typedef struct hashtable hashtable;

typedef struct {
    const UINT32* key;
    void* value;

    hashtable* _table; // reference to hash table being iterated
    size_t _index;     // current index into ht._entries
} hashtable_iterator;

hashtable* hashtable_Create(void); //creates a hashtable and returns the pointer
void* hashtable_Get(hashtable* table, const UINT32* key);

// Set item with given key (NUL-terminated) to value (which must not
// be NULL). If not already present in table, key is copied to newly
// allocated memory (keys are freed automatically when ht_destroy is
// called). Return address of copied key, or NULL if out of memory.
// const char* hashtable_Set(hashtable* table, UINT32* key, void* value);
const char* hashtable_Set(hashtable* table, const UINT32* key, void* value);

size_t hashtable_Length(hashtable* table);

// Free memory allocated for hash table, including allocated keys.
void hashtable_Destroy(hashtable* table);

// Return new hash table iterator (for use with ht_next).
hashtable_iterator hashtable_Iterator(hashtable_iterator* table);

// Move iterator to next item in hash table, update iterator's key
// and value to current item, and return true. If there are no more
// items, return false. Don't call ht_set during iteration.
boolean hashtable_Next(hashtable_iterator* it);