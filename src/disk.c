//
//  disk.c
//  
//
//  Created by Oleksandr Litus on 11/29/19.
//

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <fuse.h>

#include "utils.h"
#include "bmap.h"
#include "directory.h"

#include "disk.h"

// supeblock used to store relative location of all structures
static superblock* sblock;


/* ========================= CONSTANTS ===================================== */
const size_t ONE_MB = 1024 * 1024;
const int DIRECTORY_MODE = S_IFDIR | 0755;
const int FILE_MODE = S_IFREG | 0644;
const int SYMLINK_MODE = S_IFLNK | 0777;


/* ========================= VARIABLES ===================================== */
// pointers to disk structures in allocated memmory
static char*    imap;       // bitmap for inodes
static char*    iptr;       // inodes
static char*    dmap;       // bitmap for data blocks
static char*    dptr;       // data blocks

static int      root_ino;   // ino of the root inode


/* ========================= FUNCTIONS ===================================== */
static int      __get_free_ino();
static int      __get_free_dno();

static char*    __get_iname(const char* path);
static char*    __parent_path(const char* path);
static int      __find_ino(const char* path);

static int      __exists_inode(const char* path);
static inode*   __get_inode(const char *path);
static inode*   __create_inode(inode* dir, const char* iname, int mode);

static void     __update_stat(const inode* node, struct stat *st);

static void     __read_data(inode* file, char *buf, size_t size, off_t offset);
static int      __create_indirect_dptr();
static void     __write_data(inode* node, const char* buf,
                             size_t size, off_t offset);
static void     __truncate_up(inode* file, size_t size);
static void     __truncate_down(inode* file, size_t size);
static void     __truncate(inode* file, size_t size);

static inode*   __get_inode_from_ino(const int ino);

static int      __get_max_inum(size_t data_file_size);




/* ========================= No HELPERS =================================== */
/* Returns ino of a first free inode, if all used returns -1 */
static
int
__get_free_ino()
{
    // go through all inodes in bitmap
    for (int ino = 0; ino < sblock->inum; ino++) {
        
        // when free inode found
        if (bmap_isfree(imap, ino)) {
            
            // mark inode as used in bitmap
            bmap_set(imap, ino);
            
            // return ino
            return ino;
        }
    }
    
    // no free inodes
    return -1;
}

/* Returns dno of a dblock, otherwise -1 */
static
int
__get_free_dno()
{
    // go through all dblock in bitmap
    for (int dno = 0; dno < sblock->dnum; dno++) {
        
        // when free dblock found
        if (bmap_isfree(dmap, dno)) {
            
            // mark dblock as used in bitmap
            bmap_set(dmap, dno);
            
            // return dno
            return dno;
        }
    }
    
    // no free dblocks
    return -1;
}




/* ========================= PATH ========================================= */
/* Returns last name of an inode from the path */
static
char*
__get_iname(const char* path)
{
    const char* delim = "/";
    
    // copy path into stack
    char fullpath[strlen(path)];
    strcpy(fullpath, path);
    
    // create buffer for strtok_r
    char* restpath = NULL;
    
    // set first token
    char* curr_token = strtok_r(fullpath, delim, &restpath);
    char* next_token = strtok_r(NULL, delim, &restpath);
    
    // when next token is NULL, curr is the one we want
    while (next_token != NULL) {
        
        // get token and next token
        curr_token = next_token;
        next_token = strtok_r(NULL, delim, &restpath);
    }
    
    // get length of the iname
    size_t iname_len = strlen(curr_token);
    
    // allocate iname on the heap
    char* iname = malloc(iname_len + 1);
    assert(iname != NULL);
    iname[iname_len] = '\0';
    strcpy(iname, curr_token);

    return iname;
}

/* Returns path to the parent directory */
static
char*
__parent_path(const char* path)
{
    // get length of the path
    size_t path_len = strlen(path);
    
    printf("|--NUFS: finding parent path of %s, with length = %ld\n", // log
           path, path_len);
    
    // get the length of the iname
    char* iname = __get_iname(path);
    size_t iname_len = strlen(iname);
    
    printf("|--NUFS: last iname is %s, with length = %ld\n", // log
           iname, iname_len);
    
    // get length of the parent path
    size_t parent_len = path_len - iname_len;
    
    // allocate parent path on the heap
    char* parent = malloc(parent_len + 1);
    assert(parent != NULL);
    parent[parent_len] = '\0';
    strncpy(parent, path, parent_len);
    
    printf("|--NUFS: parent path is %s, with length %ld\n", // log
           parent, parent_len);
    
    free(iname);
    
    return parent;
}

/* Returns ino given by the path, if impossible, returns -1 */
static
int
__find_ino(const char* path)
{
    printf("|---#FUNC: __find_ino(%s)\n", path); // log
    const char* delim = "/";
    
    // check if target is root
    if (streq(path, delim)) {
        printf("|---@: path is the root with ino %d\n", root_ino); // log
        return root_ino;
    }
    
    // copy path into stack
    char fullpath[strlen(path)];
    strcpy(fullpath, path);
    
    // create buffer for strtok_r
    char* restpath = NULL;

    // get name of the target node
    char* target_name = __get_iname(fullpath);
    printf("|---> target is %s \n", target_name); // log
    
    // set first token
    char* token = strtok_r(fullpath, delim, &restpath);
    printf("|---> token is %s \n", token); // log
    
    // assign curr dir to be root
    inode* curr_dir = __get_inode_from_ino(root_ino);
    
    // declare curr_inode, and curr_ino
    inode* curr_node;
    int curr_ino;
    
    while (token != NULL) {
        
        // get curr ino from the current directory
        curr_ino = dir_get_ino(curr_dir, token);
        
        // token is not in the current directory
        if (curr_ino < 0) {
            printf("|---E: token not found in the directory\n"); // log
            return -ENOENT;
        }
        
        // get curr inode
        curr_node = __get_inode_from_ino(curr_ino);
        
        // curr inode is the target inode
        if (streq(token, target_name)) {
            printf("|---@: ino of the path is %d\n", curr_ino); // log
            return curr_ino;
        }
        
        // curr inode is a directory, continue searching
        if (is_dir(curr_node)) {
            
            // curr inode is the new directory
            curr_dir = curr_node;
            
            // get next token
            token = strtok_r(NULL, delim, &restpath);
        }
        
        // curr inode is a file, but target != node, end the loop
        else {
            break;
        }
    }
    
    free(target_name);
    
    // inode does not exist
    printf("|---E: reached end of the loop\n"); // log
    return -ENOENT;
}



/* ========================= INODE LOCAL HELPERS =========================== */
/* Checks if inode exists, returns 1 on success, and 0 on failure */
static
int
__exists_inode(const char* path)
{
    printf("|---#FUNC: __exists_inode(%s)\n", path); // log
    
    int ino = __find_ino(path);
    
    // exists
    if (ino >= 0) {
        printf("|---@: inode at path %s DOES exist and has ino %d\n", // log
               path, ino);
        return 1;
    }
    
    // does not exist
    else {
        printf("|---@: inode at path %s DOES NOT exist\n", path); // log
        return 0;
    }
}

/* Returns pointer to the inode with the given path, if such exists */
static
inode*
__get_inode(const char *path)
{
    return __get_inode_from_ino(__find_ino(path));
}

/* Creates new inode with the given mode */
static
inode*
__create_inode(inode* dir, const char* iname, int mode)
{
    printf("|--NUFS: __create_inode iname: %s\n", iname); // log
    
    int ino = __get_free_ino();
    inode* node = __get_inode_from_ino(ino);
    
    node->ino = ino;
    node->mode = mode;
    
    node->size = 0;
    node->nlink = 1;
    node->dptrs[0] = __get_free_dno();
    
    for (int ii = 1; ii < BLOCKS_NUM; ++ii) {
        node->dptrs[ii] = -1;
    }
    
    node->indirect_dptr = -1;
    node->dnum = 1;
    
    node->uid = getuid();
    node->gid = getgid();
    
    time_t tt = time(NULL);
    node->atime = tt;
    node->ctime = tt;
    node->mtime = tt;
    
    // root is created
    if (dir == NULL && iname == NULL) {
        dir_init(node, node->ino);
        return node;
    }
    
    // non root directory is created
    if (mode == DIRECTORY_MODE) {
        dir_init(node, dir->ino);
    }
    
    dir_add_inode(dir, node->ino, iname);
    
    return node;
}




/* ========================= ATRIBUTES ===================================== */
/* Updates given "st" with atributes of inode */
static
void
__update_stat(const inode* node, struct stat *st)
{
    st->st_rdev     = 0;
    st->st_ino      = node->ino;
    st->st_mode     = node->mode;
    st->st_nlink    = node->nlink;
    
    st->st_uid      = node->uid;
    st->st_gid      = node->gid;
    
    st->st_size     = node->size;
    st->st_blksize  = BLOCK_SIZE;
    st->st_blocks   = node->dnum * 8; // convert to 512 blocks
    
    struct timespec atime_spec;
    atime_spec.tv_sec = node->atime;
    atime_spec.tv_nsec = node->atime * 1000 * 1000 * 1000; // nanoseconds

    struct timespec mtime_spec;
    mtime_spec.tv_sec = node->mtime;
    mtime_spec.tv_nsec = node->mtime * 1000 * 1000 * 1000; // nanoseconds

    struct timespec ctime_spec;
    ctime_spec.tv_sec = node->ctime;
    ctime_spec.tv_nsec = node->ctime * 1000 * 1000 * 1000; // nanoseconds
    
    st->st_atim     = atime_spec;
    st->st_ctim     = mtime_spec;
    st->st_mtim     = ctime_spec;
}




/* ========================= LOCATION ====================================== */
/* Returns pointer to the data block with the given dno */
dblock*
disk_get_dblock(const int dno)
{
    return ((dblock*)dptr) + dno;
}


/* Returns pointer to the inode with the given ino */
static
inode*
__get_inode_from_ino(const int ino)
{
    return ((inode*)iptr + ino);
}




/* ==================== INODE ============================================== */
/* Checks if a file exists, returns 0 on success, and -ENOENT on failure */
int
disk_access(const char *path)
{
    printf("|---#FUNC: disk_access(%s)\n", path); // log
    
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    // update time stamps
    inode* node = __get_inode(path);
    time_t tt = time(NULL);
    node->atime = tt;
    
    printf("|---@: done\n"); // log
    
    // inode exists
    return 0;
}

/* Gets object's attributes into "st" */
int
disk_getattr(const char *path, struct stat *st)
{
    printf("|---#FUNC: disk_getattr(%s)\n", path); // log
    
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;

    // inode exists
    inode* node = __get_inode(path);
    printf("|---> got inode with ino %d\n", node->ino); // log
    
    // update time stamps
    time_t tt = time(NULL);
    node->atime = tt;
    
    // update "st" struct
    __update_stat(node, st);
    printf("|---> updated stat struct\n"); // log
    
    printf("|---@: done\n"); // log
    
    return 0;
}

/* Creates an inode with the given mode */
int
disk_mknod(const char *path, int mode)
{
    // find parent directory path
    char* dir_path = __parent_path(path);
    
    // parent direcory does not exist
    if (!__exists_inode(dir_path)) return -ENOENT;
    
    // inode already exists
    if (__exists_inode(path)) return -EEXIST;
    
    // get parent directory inode
    inode* dir = __get_inode(dir_path);
    
    // get iname of the node to be created
    char* iname = __get_iname(path);
    printf("|--NUFS: iname: %s\n", iname); // log
    
    // node is a directory
    if (S_ISDIR(mode)) {
        printf("|--NUFS: node is a directory: %s\n", iname); // log
        __create_inode(dir, iname, DIRECTORY_MODE);
    }
    
    // node is a file
    else {
        printf("|--NUFS: node is a file: %s\n", iname); // log
        __create_inode(dir, iname, FILE_MODE);
    }
    
    free(iname);
    free(dir_path);
    
    return 0;
}


/* Moves node "from" "to" and renames */
int
disk_rename(const char *from, const char *to)
{
    // "from" inode does not exist
    if (!__exists_inode(from)) return -ENOENT;
    
    // "to" inode does exist
    if (__exists_inode(to)) return -EEXIST;

    // get path to the old directory
    char* old_dir_path = __parent_path(from);
    inode* old_dir = __get_inode(old_dir_path);
    
    // find file ino
    int file_ino = __find_ino(from);
    
    // delete file from the old directory
    dir_delete_inode(old_dir, file_ino);

    // get path to the new directory
    char* new_dir_path = __parent_path(to);
    
    // new directory does not exist
    if (!__exists_inode(new_dir_path)) return -ENOENT;

    inode* new_dir = __get_inode(new_dir_path);

    // get new filename
    char* new_filename = __get_iname(to);
    
    // add file to the new directory
    dir_add_inode(new_dir, file_ino, new_filename);
    
    free(new_filename);
    free(old_dir_path);
    free(new_dir_path);
    
    inode* node = __get_inode(from);
    
    // update time stamps
    time_t tt = time(NULL);
    node->atime = tt;
    node->mtime = tt;
    
    return 0;
}

/* Changes mode of the node */
int
disk_chmod(const char *path, mode_t mode)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;

    inode* node = __get_inode(path);
    
    node->mode = mode;
    
    // update time stamps
    time_t tt = time(NULL);
    node->atime = tt;
    node->mtime = tt;
    
    return 0;
}


/* Updates timestamps of the inode */
int
disk_utimens(const char* path, const struct timespec ts[2])
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    inode* node = __get_inode(path);
    node->atime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;
    
    return 0;
}




/* ==================== FILE =============================================== */
/* Creates a hard link to existing file */
int
disk_link(const char *from, const char *to)
{
    // "from" inode does not exist
    if (!__exists_inode(from)) return -ENOENT;
    
    // "to" inode does exist
    if (__exists_inode(to)) return -EEXIST;

    // get path to the new directory
    char* new_dir_path = __parent_path(to);
    
    // new directory does not exist
    if (!__exists_inode(new_dir_path)) return -ENOENT;

    // get new directory node
    inode* new_dir = __get_inode(new_dir_path);
    
    // get new filename and file ino
    int file_ino = __find_ino(from);
    char* new_filename = __get_iname(to);
    
    // update number of hard links in the node
    inode* file = __get_inode(from);
    file->nlink += 1;
    
    // add hard link to the new directory
    dir_add_inode(new_dir, file_ino, new_filename);
    
    free(new_filename);
    free(new_dir_path);
    
    // update time stamps
    time_t tt = time(NULL);
    file->atime = tt;
    file->mtime = tt;
    
    return 0;
}

/* Deletes inode from the file system */
static
void
__delete_inode(inode* node)
{
    // free inode from the bitmap
    bmap_free(imap, node->ino);
    
    // free all direct inode data blocks
    for (int ii = 0; ii < BLOCKS_NUM; ++ii) {
        
        int dno = node->dptrs[ii];
        
        // reached free data blocks
        if (dno < 0) {
            break;
        }
        
        // data block is not free
        else {
            bmap_free(dmap, dno);
        }
    }
    
    // no indirect data blocks
    if (node->indirect_dptr < 0) {
        return;
    }
    
    // free all inderect inode data blocks
    int* indirect_dptrs = (int*)disk_get_dblock(node->indirect_dptr);
    
    int indir_blocks_count = BLOCK_SIZE / sizeof(int);
    for (int ii = 0; ii < indir_blocks_count; ++ii) {
        
        int dno = indirect_dptrs[ii];
        
        // reached free data blocks
        if (dno < 0) {
            break;
        }
        
        // data block is not free
        else {
            bmap_free(dmap, dno);
        }
    }
    
    // free the indirect pointer itself
    bmap_free(dmap, node->indirect_dptr);
}

/* Removes a link to file, when last link removed, deletes a file */
int
disk_unlink(const char *path)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;

    inode* file = __get_inode(path);
    
    // get path to the directory
    char* dir_path = __parent_path(path);
    inode* dir = __get_inode(dir_path);
    
    // delete hard link from the directory
    dir_delete_inode(dir, file->ino);
    
    // decrement number of hard links to the file
    file->nlink -= 1;
    
    // if no hard links exist, file is deleted
    if (file->nlink == 0) {
        __delete_inode(file);
    }
    
    free(dir_path);
    
    // update time stamps
    time_t tt = time(NULL);
    file->atime = tt;
    file->mtime = tt;
    
    return 0;
}

/* Reads data from the file */
static
void
__read_data(inode* file, char *buf, size_t size, off_t offset)
{
    // find the starting data block
    int start_block = offset / BLOCK_SIZE;
    printf("|---> start_block = %d\n", start_block); // log
    
    size_t curr = 0;
    size_t left = size;
    off_t read_off = 0;
    off_t off = offset % BLOCK_SIZE;
    printf("|---> off = %ld\n", off); // log
    
    // iterating through data blocks in the file
    for (int ii = start_block; ii < file->dnum; ++ii) {
        
        // decrement left by how much was read on previus itteration
        left -= curr;
        
        // if nothing is left, exit
        if (left == 0) break;
        
        // increment offset by how much was read on previus itteration
        read_off += curr;
        printf("|---> read_off = %ld\n", read_off); // log
        
        // update curr to be BLOCK_SIZE, or what is left
        curr = (left > BLOCK_SIZE) ? BLOCK_SIZE : left;
        
        // need to find next data block number
        int dno;
        
        // there still free data blocks in standart array
        if (ii < BLOCKS_NUM) {
            
            // get next data block number
            dno = file->dptrs[ii];
        }
        
        // no more data data block left in standart array
        else {
            
            // get next data block number
            int* indirect_dptrs = (int*)disk_get_dblock(file->indirect_dptr);
            dno = indirect_dptrs[ii - BLOCKS_NUM];
        }
        
        printf("|---> dno = %ld\n", dno); // log
        
        // get next data block
        dblock* block = disk_get_dblock(dno);

        // copy data into current "buf"
        memcpy(buf + read_off, block->data + off, curr);
        
        // make sure in the next block all data is read
        off = 0;
    }
}

/* Reads data from the file into "buf" */
int
disk_read(const char *path, char *buf, size_t size, off_t offset)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    inode* file = __get_inode(path);
    
    // read data from the file
    __read_data(file, buf, size, offset);
    
    // update time stamps
    time_t tt = time(NULL);
    file->atime = tt;
    
    return size;
}

/* Creates indirect pointer to data blocks */
static
int
__create_indirect_dptr()
{
    int indirect_dptr = __get_free_dno();
    int* ptrs = (int*)disk_get_dblock(indirect_dptr);
    
    int ptrs_count = BLOCK_SIZE / sizeof(int);
    
    for (int ii = 0; ii < ptrs_count; ++ii) {
        ptrs[ii] = -1;
    }
    
    return indirect_dptr;
}

/* Writes data into the file */
static
void
__write_data(inode* file, const char* buf, size_t size, off_t offset)
{
    // find the starting data block
    int start_block = (int)offset / BLOCK_SIZE;
    printf("|---> start_block = %d\n", start_block); // log
    
    size_t curr = 0;
    size_t left = size;
    off_t written_off = 0;
    off_t off = offset % BLOCK_SIZE;
    printf("|---> off = %ld\n", off); // log
    
    // iterating through data blocks in the file
    for (int ii = start_block; ; ++ii) {
        
        // decrement left by how much was written on previus itteration
        left -= curr;
        
        // if nothing is left, exit
        if (left == 0) break;
        
        // increment offset by how much was written on previus itteration
        written_off += curr;
        printf("|---> written_off = %ld\n", written_off); // log
        
        // update curr to be BLOCK_SIZE, or what is left
        curr = (left > BLOCK_SIZE) ? BLOCK_SIZE : left;
        
        // need to find next data block number
        int dno;
        
        // there still free data blocks in standart array
        if (ii < BLOCKS_NUM) {
            
            // get next data block number
            dno = file->dptrs[ii];
            
            // if data block is not assigned, get one
            if (dno < 0) {
                dno = __get_free_dno();
                file->dptrs[ii] = dno;
                file->dnum += 1;
            }
        }
        
        // no more data data block left in standart array
        else {
            
            // there is no indirect dptr
            if (file->indirect_dptr < 0) {
                file->indirect_dptr = __create_indirect_dptr();
                printf("|---> created new indirect pointer (%d)\n", // log
                       file->indirect_dptr);
            }
            
            int* indirect_dptrs = (int*)disk_get_dblock(file->indirect_dptr);
            dno = indirect_dptrs[ii - BLOCKS_NUM];
            printf("|---> addr[%d]\n", ii - BLOCKS_NUM); // log
            
            // if data block is not assigned, get one
            if (dno < 0) {
                dno = __get_free_dno();
                indirect_dptrs[ii - BLOCKS_NUM] = dno;
                file->dnum += 1;
            }
        }
        
        printf("|---> dno = %ld\n", dno); // log
        
        // get next data block
        dblock* block = disk_get_dblock(dno);

        // copy data into current data block from "buf"
        memcpy(block->data + off, buf + written_off, curr);
        
        // make sure in the next block all data is written
        off = 0;
    }
}

/* Writes data from "buf" into the file */
int
disk_write(const char *path, const char *buf, size_t size, off_t offset)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    inode* file = __get_inode(path);
    
    // write new data into the file
    __write_data(file, buf, size, offset);
    
    // update file stat
    file->size = file->size + size;
    
    // update time stamps
    time_t tt = time(NULL);
    file->atime = tt;
    file->mtime = tt;
    
    return size;
}

/* Free extra data blocks */
static
void
__truncate_down(inode* file, size_t size)
{
    // find the starting data block
    int new_size = (int)(file->size - size);
    int start_block = new_size / BLOCK_SIZE;
    printf("|---> start_block = %d\n", start_block); // log
    
    // free all direct inode data blocks after start block
    for (int ii = start_block; ii < BLOCKS_NUM; ++ii) {
        
        int dno = file->dptrs[ii];
        
        // reached free data blocks
        if (dno < 0) {
            return;
        }
        
        // data block is not free
        else {
            bmap_free(dmap, dno);
        }
    }
    
    // no indirect data blocks
    if (file->indirect_dptr < 0) {
        return;
    }
    
    // blocks starts after direct pointers
    int indi_start_block;
    if (start_block > BLOCKS_NUM) {
        indi_start_block = start_block;
    }
    
    // blocks started somewhere in direct pointers
    else {
        indi_start_block = 0;
    }
    
    // free all inderect inode data blocks after start_block or 0
    int* indirect_dptrs = (int*)disk_get_dblock(file->indirect_dptr);
    
    int indir_blocks_count = BLOCK_SIZE / sizeof(int);
    for (int ii = indi_start_block; ii < indir_blocks_count; ++ii) {
        
        int dno = indirect_dptrs[ii];
        
        // reached free data blocks
        if (dno < 0) {
            break;
        }
        
        // data block is not free
        else {
            bmap_free(dmap, dno);
        }
    }
    
    if (indi_start_block == 0) {
        // free the indirect pointer itself
        bmap_free(dmap, file->indirect_dptr);
    }
}

/* Add enough data blocks */
static
void
__truncate_up(inode* file, size_t size)
{
    size_t new_size = file->size - size;
    char* buf = malloc(new_size);
    assert(buf != NULL);
    
    int offset = file->size;
    
    // fill buffer with zeroes
    for (int ii = 0; ii < new_size; ++ii) {
        buf[ii] = '\0';
    }
    
    __write_data(file, buf, new_size, offset);
    
    free(buf);
}

/* Truncate file to the given size */
static
void
__truncate(inode* file, size_t size)
{
    // truncate to bigger size
    if (size == file->size) {
        return;
    }
    
    // truncate to smaller size
    if (size < file->size) {
        __truncate_down(file, size);
    }
    
    // truncate to bigger size
    __truncate_up(file, size);
}

/* Truncates file to the given size */
int
disk_truncate(const char *path, off_t size)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    inode* file = __get_inode(path);
    
    // truncate the file
    __truncate(file, size);
    
    // update file stat
    file->size = size;
    
    // update time stamps
    time_t tt = time(NULL);
    file->atime = tt;
    file->mtime = tt;
    
    return 0;
}


/* ==================== DIRECTORY ========================================== */
/* Creates new directory */
int disk_mkdir(const char *path, mode_t mode)
{
    return disk_mknod(path, DIRECTORY_MODE);
}

/* Removes directory */
int disk_rmdir(const char *path)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    // get parent directory inode
    char* parent_dir_path = __parent_path(path);
    inode* parent_dir = __get_inode(parent_dir_path);
    
    // get directory inode
    inode* dir = __get_inode(path);
    
    // directory is empty, delete it
    if (dir->size == 0) {
        dir_delete_inode(parent_dir, dir->ino);
        // TODO: clean data blocks
        return 0;
    }
    
    free(parent_dir_path);
    
    // directory is not empty
    return -ENOTEMPTY;
}

/* Lists the contents of a directory using "filler" into "buf" */
int
disk_readdir(const char *path, void *buf, fuse_fill_dir_t filler)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    // inode exists
    inode* dir = __get_inode(path);
    
    // create structrure for attributes
    struct stat st;
    
    // update "st" struct
    __update_stat(dir, &st);
   
    // add dir to the list
    filler(buf, ".", &st, 0);
    
    // get all entries in the directory
    dblock* block = disk_get_dblock(dir->dptrs[0]);
    dentry* entries = (dentry*)block;
    
    int dentry_count = BLOCK_SIZE / sizeof(dentry);
    
    for (int ii = 0; ii < dentry_count; ++ii) {
        
        if (entries[ii].ino != -1) {
            inode* node = __get_inode_from_ino(entries[ii].ino);
            __update_stat(node, &st);
            filler(buf, entries[ii].iname, &st, 0);
        }
    }
    
    // update time stamps
    time_t tt = time(NULL);
    dir->atime = tt;
    
    return 0;
}


/* ========================= SYMLINKS ====================================== */
/* Creates symlink "from" "to" */
int
disk_symlink(const char *from, const char *to)
{
    // "from" inode does not exist
    if (!__exists_inode(from)) return -ENOENT;
    
    // "to" inode does exist
    if (__exists_inode(to)) return -EEXIST;

    // get path to the new directory
    char* new_dir_path = __parent_path(to);
    
    // new directory does not exist
    if (!__exists_inode(new_dir_path)) return -ENOENT;

    // get new directory node
    inode* new_dir = __get_inode(new_dir_path);
    
    // get new filename
    char* new_filename = __get_iname(to);
    
    // create symbolic link
    inode* file = __create_inode(new_dir, new_filename, SYMLINK_MODE);
    
    // save the link intp file data
    dblock* block = disk_get_dblock(file->dptrs[0]);
    strcpy(block->data, from);
    file->size = strlen(from);
    
    free(new_filename);
    free(new_dir_path);
    
    return 0;
}

/* Reads symlink */
int
disk_readlink(const char *path, char *buf, size_t size)
{
    // inode does not exist
    if (!__exists_inode(path)) return -ENOENT;
    
    inode* file = __get_inode(path);
    
    // read data from the file
    dblock* block = disk_get_dblock(file->dptrs[0]);
    strncpy(buf, block->data, size);
    
    // update time stamps
    time_t tt = time(NULL);
    file->atime = tt;
    
    return 0;
}




/* ========================= MOUNT DISK ==================================== */
/* Reinitialize NUFS with given data_file */
static
void
remount_disk(const char* data_file)
{
    printf("|--NUFS: Start reinitializing old data file at %s\n", // log
           data_file);
    
    struct stat st;
    int rv = stat(data_file, &st);
    assert(rv != -1);
    size_t data_file_size = st.st_size;
    assert(data_file_size <= ONE_MB);
    printf("|--NUFS: Old data file size is %ld\n", data_file_size); // log
    
    // open data file
    int fd = open(data_file, O_RDWR, 0644);
    assert(fd != -1);
    printf("|--NUFS: Opened old data file\n"); // log
    
    // truncate data file to 1MB
    rv = ftruncate(fd, ONE_MB);
    assert(rv != -1);
    printf("|--NUFS: Truncated old data file to %ld\n", ONE_MB); // log
    
    // mmap data file into memory
    sblock = mmap(NULL, data_file_size,
                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(sblock != NULL);
    printf("|--NUFS: Mmaped old data file in memmory\n"); // log
    
    // close file descriptor of the data file
    rv = close(fd);
    assert(rv != -1);
    printf("|--NUFS: Closed file descriptor of the old data file\n"); // log
    
    // update relative pointers
    imap = sblock->imap + (char*)sblock;
    iptr = sblock->iptr + (char*)sblock;
    dmap = sblock->dmap + (char*)sblock;
    dptr = sblock->dptr + (char*)sblock;
    printf("|--NUFS: Updated relative pointers\n"); // log
    
    // update root pointer
    root_ino = sblock->root_ino;
    inode* root = __get_inode_from_ino(root_ino);
    root->atime = time(NULL);
    printf("|--NUFS: Updated root pointer\n"); // log
}

/* Calculate number of inodes in the FS of the given data file size */
static
int
__get_max_inum(size_t data_file_size)
{
    return (data_file_size * 4 - sizeof(superblock) * 4)
    /(sizeof(inode) * 4 + sizeof(dblock) * 4 + 2);
}

/* Creates new disk for NUFS (1MB size) at given data_file path */
static
void
create_disk(const char* data_file)
{
    // update NUFS LOG
    printf("|--NUFS: Start creation of new disk at %s\n", data_file); // log
    
    size_t data_file_size = ONE_MB;
    
    // create data file
    int fd = open(data_file, O_RDWR | O_CREAT, 0644);
    assert(fd != -1);
    printf("|--NUFS: Created new data file\n"); // log
    
    // truncate data file to 1MB
    int rv = ftruncate(fd, data_file_size);
    assert(rv != -1);
    printf("|--NUFS: Truncated new data file to %ld\n", data_file_size); // log
    
    // mmap data file into memory
    sblock = mmap(NULL, data_file_size,
                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(sblock != NULL);
    printf("|--NUFS: Mmaped data file into memory\n"); // log
    
    // close file descriptor of the data file
    rv = close(fd);
    assert(rv != -1);
    printf("|--NUFS: Closed data file\n"); // log
    
    // get number of inodes and data blocks
    sblock->inum = __get_max_inum(data_file_size);
    sblock->dnum = sblock->inum;
    printf("|--NUFS: Calculated number of inodes: %d\n",
           sblock->inum); // log
    printf("|--NUFS: Calculated number of data blocks: %d\n",
           sblock->dnum); // log
    
    // calculate sizes of bitmaps and iptr region
    size_t imap_size = div_up(sblock->inum, 4);
    size_t dmap_size = div_up(sblock->dnum, 4);
    size_t iptr_size = sblock->inum * sizeof(inode);
    printf("|--NUFS: Calculated imap_size: %ld\n", imap_size); // log
    printf("|--NUFS: Calculated dmap_size: %ld\n", dmap_size); // log
    printf("|--NUFS: Calculated iptr_size: %ld\n", iptr_size); // log
    
    // get pointers to the virtual disk adresses
    imap = ((char*)sblock) + sizeof(superblock);
    dmap = imap + imap_size;
    iptr = dmap + dmap_size;
    dptr = iptr + iptr_size;
    printf("|--NUFS: Got pointers to virtual disk\n"); // log
    
    // update relative pointers in superblock
    sblock->imap = imap - (char*)sblock;
    sblock->dmap = dmap - (char*)sblock;
    sblock->iptr = iptr - (char*)sblock;
    sblock->dptr = dptr - (char*)sblock;
    printf("|--NUFS: Updated relative pointers\n"); // log
    
    // initialize bitmaps
    bmap_init(imap, imap_size);
    bmap_init(dmap, dmap_size);
    
    // create root inode
    inode* root = __create_inode(NULL, NULL, DIRECTORY_MODE);
    sblock->root_ino = root->ino;
    root_ino = root->ino;
    printf("|--NUFS: Created new root with ino %d\n", root_ino); // log
    
    // initialize root directory
    // dir_init(root, root_ino);
    // printf("|--NUFS: Initialized new root as directory\n"); // log
    
    printf("|--NUFS: Created new disk\n"); // log
}

/* Initializes disk for NUFS in data_file */
void
disk_mount(const char* data_file)
{
    int rv = access(data_file, F_OK);
    
    // data file exists
    if (rv == 0) {
        printf("|--NUFS: Data file %s DOES exist\n", data_file); // log
        remount_disk(data_file);
    }
    
    // data file does not exist
    else {
        printf("|--NUFS: Data file %s DOES NOT exists\n", data_file); // log
        create_disk(data_file);
    }
}
