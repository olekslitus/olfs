//
//  disk.h
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#ifndef disk_h
#define disk_h

#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <fuse.h>

#define BLOCK_SIZE 4096
#define BLOCKS_NUM 3


/* ========================= STRUCTURES =================================== */
/* Holds all relative pointers */
typedef struct superblock {
    ptrdiff_t       imap;       // relative pointer to bitmap for inodes
    ptrdiff_t       iptr;       // relative pointer to the inodes array
    int             inum;       // total number of inodes
    
    ptrdiff_t       dmap;       // relative pointer to bitmap for data blocks
    ptrdiff_t       dptr;       // relative pointer to the data block array
    int             dnum;       // total number of data blocks
    
    int             root_ino;   // ino of the root directory
} superblock;


/* Inode - basic file structure */
typedef struct inode {
    int         ino;            // inode number (id)
    int         mode;           // permission & type
    int         size;           // bytes
    
    int         uid;            // user id
    int         gid;            // group id
    
    int         atime;          // last access time
    int         ctime;          // creation time
    int         mtime;          // last modification time
    
    int         nlink;          // number of hard links pointing to this file
    int         dnum;           // number of data block allocated
    
    int         dptrs[BLOCKS_NUM];      // direct data block pointers
    int         indirect_dptr;          // single indirect pointer
} inode;


/* Data block */
typedef struct dblock {
    char data[BLOCK_SIZE];
} dblock;


/* ========================= FUNCTIONS ==================================== */
void    disk_mount(const char* data_file);
dblock* disk_get_dblock(const int dno);

int disk_access(const char *path);
int disk_getattr(const char *path, struct stat *st);
int disk_mknod(const char *path, int mode);
int disk_rename(const char *from, const char *to);
int disk_chmod(const char *path, mode_t mode);
int disk_utimens(const char* path, const struct timespec ts[2]);

int disk_link(const char *from, const char *to);
int disk_unlink(const char *path);
int disk_read(const char *path, char *buf, size_t size, off_t offset);
int disk_write(const char *path, const char *buf, size_t size, off_t offset);
int disk_truncate(const char *path, off_t size);

int disk_mkdir(const char *path, mode_t mode);
int disk_rmdir(const char *path);
int disk_readdir(const char *path, void *buf, fuse_fill_dir_t filler);

int disk_symlink(const char *from, const char *to);
int disk_readlink(const char *path, char *buf, size_t size);

#endif /* disk_h */
