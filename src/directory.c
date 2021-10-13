//
//  directory.c
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#include <sys/stat.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "utils.h"
#include "disk.h"

#include "directory.h"


static const int DENTRY_COUNT =  BLOCK_SIZE / sizeof(dentry);
static void dir_print(inode* dir);


/* Initializes directory */
void
dir_init(inode* dir, int parent_ino)
{
    assert(dir != NULL);
    assert(parent_ino >= 0);
    
    dblock* block = disk_get_dblock(dir->dptrs[0]);
    dentry* entries = (dentry*)block;
    
    // add perent directory
    strcpy(entries[0].iname, "..");
    entries[0].ino = parent_ino;
    
    // clean all other entries
    for (int ii = 1; ii < DENTRY_COUNT; ++ii) {
        entries[ii].ino = -1;
    }
}

/* Is given inode a directory? */
int
is_dir(inode* node)
{
    assert(node != NULL);
    
    return S_ISDIR(node->mode);
}

/* Returns ino of the inode with iname, otherwise -1 */
int
dir_get_ino(inode* dir, const char* iname)
{
    assert(dir != NULL);
    assert(iname != NULL);
    
    printf("| --- iname: %s\n", iname); // log
    
    dir_print(dir); // log
    
    dblock* block = disk_get_dblock(dir->dptrs[0]);
    dentry* entries = (dentry*)block;
    
    for (int ii = 0; ii < DENTRY_COUNT; ++ii) {
        if (streq(entries[ii].iname, iname)) {
            return entries[ii].ino;
        }
    }
    
    return -1;
}

/* Adds inode to the directory */
void
dir_add_inode(inode* dir, int ino, const char* iname)
{
    assert(dir != NULL);
    assert(iname != NULL);
    assert(ino >= 0);
    
    printf("| --- iname: %s\n", iname); // log
    
    dblock* block = disk_get_dblock(dir->dptrs[0]);
    dentry* entries = (dentry*)block;
    
    for (int ii = 0; ii < DENTRY_COUNT; ++ii) {
        if (entries[ii].ino == -1) {
            strcpy(entries[ii].iname, iname);
            entries[ii].ino = ino;
            
            printf("| --- iname: %s, ino: %d\n",     // log
                   entries[ii].iname, entries[ii].ino);
            
            return;
        }
    }
}

/* Deletes inode from the directory */
void
dir_delete_inode(inode* dir, int ino)
{
    assert(dir != NULL);
    assert(ino >= 0);
    
    dblock* block = disk_get_dblock(dir->dptrs[0]);
    dentry* entries = (dentry*)block;
    
    for (int ii = 0; ii < DENTRY_COUNT; ++ii) {
        if (entries[ii].ino == ino) {
            entries[ii].ino = -1;
            return;
        }
    }
}

/* Prints out directory content */
static
void
dir_print(inode* dir)
{
    assert(dir != NULL);
    
    dblock* block = disk_get_dblock(dir->dptrs[0]);
    dentry* entries = (dentry*)block;
    
    // change to DENTRY_COUNT to print all
    int max = 5;
    for (int ii = 0; ii < max; ++ii) {
        printf("| --- iname: %s, ino: %d\n",     // log
               entries[ii].iname, entries[ii].ino);
    }
}
