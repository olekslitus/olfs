//
//  bmap.c
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#include <assert.h>

#include "bmap.h"


/* ==================== LOCAL HELPERS ===================================== */
/* Gets the value of the bit at "pos" */
static
int
bmap_get(void* bmap, int pos)
{
    assert(bmap != NULL);
    assert(pos >= 0);
    
    // find the word in the map
    char* word = ((char *) bmap + pos / 8);
    
    // calc the index of the bit in the word (from 0 to 7)
    int word_index = pos % 8;
    
    // return the value of the bit
    return (*word >> word_index) & 1;
}

/* Puts the first bit of the "val" to the map at "pos" */
static
void
bmap_put(void* bmap, const int pos, const int val)
{
    assert(bmap != NULL);
    assert(pos >= 0);
    assert(val == 0 || val == 1);
    
    // change val to its first bit
    int bval = val & 1;
    
    // find the word in the map
    char* word = ((char *) bmap + pos / 8);
    
    // calc the index of the bit in the word (from 0 to 7)
    int word_index = pos % 8;
    
    // set the bit to the val
    *word = (*word & ~(1 << word_index)) | (bval << word_index);
}




/* ==================== FUNCTIONS ========================================= */
/* Returns 1 if the "pos" represents a free entry */
int
bmap_isfree(void* bmap, const int pos)
{
    assert(bmap != NULL);
    assert(pos >= 0);

    return (bmap_get(bmap, pos) == 0);
}

/* Puts 1 to the map at "pos" */
void
bmap_set(void* bmap, const int pos)
{
    assert(bmap != NULL);
    assert(pos >= 0);
    
    bmap_put(bmap, pos, 1);
}

/* Puts 0 to the map at "pos" */
void
bmap_free(void* bmap, const int pos)
{
    assert(bmap != NULL);
    assert(pos >= 0);
    
    bmap_put(bmap, pos, 0);
}

/* Inializes "bmap" will vals set to 0 */
void
bmap_init(void* bmap, const int size)
{
    assert(bmap != NULL);
    assert(size > 0);
    
    // free all the entries
    for (int ii = 0; ii < size; ++ii) {
        bmap_free(bmap, ii);
    }
}
