//
//  bmap.h
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#ifndef bmap_h
#define bmap_h

#include <stdio.h>

void    bmap_init(void* bmap, const int size);
int     bmap_isfree(void* bmap, const int pos);
void    bmap_set(void* bmap, const int pos);
void    bmap_free(void* bmap, const int pos);

#endif /* bmap_h */
