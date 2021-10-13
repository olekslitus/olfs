//
//  directory.h
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#ifndef directory_h
#define directory_h

#include <stdio.h>
#include "disk.h"

typedef struct dentry {
    char iname[48];
    int  ino;
    char _reserved[12];
} dentry;

void    dir_init(inode* dir, int parent_ino);
int     is_dir(inode* node);
int     dir_get_ino(inode* dir, const char* iname);
void    dir_delete_inode(inode* dir, int ino);
void    dir_add_inode(inode* dir, int ino, const char* iname);
dentry* dir_get_dentry(inode* dir);

#endif /* directory_h */
