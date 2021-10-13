//
//  utils.c
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#include <string.h>
#include <assert.h>

#include "utils.h"

/* Divides two size_t and rounds up */
size_t
div_up(const size_t aa, const size_t bb)
{
    assert(bb != 0);
    
    return ((aa - 1) / bb) + 1;
}

/* Are two strings equal? */
int
streq(const char* aa, const char* bb)
{
    assert(aa != NULL);
    assert(bb != NULL);
    
    return strcmp(aa, bb) == 0;
}

/* Returns minimum of two ints */
int
min(const int x, const int y)
{
    return (x < y) ? x : y;
}
