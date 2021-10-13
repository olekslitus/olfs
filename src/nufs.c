// based on cs3650 starter code
// completed by Oleksandr Litus

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "utils.h"
#include "directory.h"
#include "disk.h"


/* ==================== INODE ============================================== */
/* Checks if an inode exists, returns 0 on success, and -ENOENT on failure */
int
nufs_access(const char *path, int mode)
{
    printf("#SYSCALL: access(%s, %04o)\n", path, mode); // log
    
    int rv = disk_access(path);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Gets inode's attributes into "st" */
int
nufs_getattr(const char *path, struct stat *st)
{
    printf("#SYSCALL: getattr(%s)\n", path); // log
    
    int rv = disk_getattr(path, st);
    
    printf("@->: (%d) {mode: %04o, size: %ld}\n\n\n",
           rv, st->st_mode, st->st_size); // log
    
    return rv;
}

/* Creates inode with the given mode */
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("#-SYSCALL: mknod(%s, %04o)\n", path, mode); // log
    
    int rv = disk_mknod(path, mode);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Renames and moves inode "from" "to" */
int
nufs_rename(const char *from, const char *to)
{
    printf("#-SYSCALL: rename(%s => %s)\n", from, to); // log
    
    int rv = disk_rename(from, to);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Changes mode of an inode */
int
nufs_chmod(const char *path, mode_t mode)
{
    printf("#-SYSCALL: unlink(%s)\n", path); // log
    
    int rv = disk_chmod(path, mode);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Update the timestamps on inode */
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
    printf("#-SYSCALL: utimens(%s, [%ld, %ld; %ld %ld])\n", // log
           path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec);
    
    int rv = disk_utimens(path, ts);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}




/* ==================== FILE =============================================== */
/* Checks if file exist */
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    printf("#-SYSCALL: open(%s)\n", path); // log
    
    int rv = disk_access(path);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return 0;
}

// TODO: implements: man 2 link
/* Creates a new hard link to existing file */
int
nufs_link(const char *from, const char *to)
{
    printf("#-SYSCALL: link(%s => %s)\n", from, to); // log
    
    int rv = disk_link(from, to);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Deletes a file or hard link */
int
nufs_unlink(const char *path)
{
    printf("#-SYSCALL: unlink(%s)\n", path); // log
    
    int rv = disk_unlink(path);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Reads data from the file into "buf", returns number of bytes read */
int
nufs_read(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fi)
{
    
    printf("#-SYSCALL: read(%s, %ld bytes, @+%ld)\n", // log
           path, size, offset);
    
    int rv = disk_read(path, buf, size, offset);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Writes data from "buf" to the file, returns number of bytes written */
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi)
{
    printf("#-SYSCALL: write(%s, %ld bytes, @+%ld)\n", // log
           path, size, offset);
    
    int rv = disk_write(path, buf, size, offset);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Truncates file to a specific size */
int
nufs_truncate(const char *path, off_t size)
{
    printf("#-SYSCALL: truncate(%s, %ld bytes)\n", path, size); // log
    
    int rv = disk_truncate(path, size);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return 0;
}




/* ==================== DIRECTORY ========================================== */
/* Creates a directory */
int
nufs_mkdir(const char *path, mode_t mode)
{
    printf("#-SYSCALL: mkdir(%s)\n", path); // log
    
    int rv = disk_mkdir(path, mode);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Removes a directory */
int
nufs_rmdir(const char *path)
{
    printf("#-SYSCALL: rmdir(%s)\n", path); // log
    
    int rv = disk_rmdir(path);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Lists the contents of a directory using "filler" into "buf" */
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    printf("#-SYSCALL: readdir(%s)\n", path); // log
    
    int rv = disk_readdir(path, buf, filler);
     
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}




/* ==================== SYMLINKS =========================================== */
/* Creates a symbolic link to the file */
int
nufs_symlink(const char *from, const char *to)
{
    printf("#-SYSCALL: symlink(%s, %s)\n", from, to); // log
    
    int rv = disk_symlink(from, to);
     
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}

/* Reads a symbolic link */
int
nufs_readlink(const char *path, char *buf, size_t size)
{
    printf("#-SYSCALL: write(%s, %ld bytes)\n", path, size); // log
    
    int rv = disk_readlink(path, buf, size);
    
    printf("@->: %d\n\n\n", rv); // log
    
    return rv;
}




/* ==================== NUFS GENERAL ======================================= */
/* Initialiaze FUSE operations as NUFS functions */
void
nufs_init_fuse_opers(struct fuse_operations* opers)
{
    // clean the FUSE operations struct
    memset(opers, 0, sizeof(struct fuse_operations));
    
    // assign NUFS functions to FUSE operations
    opers->access   = nufs_access;
    opers->getattr  = nufs_getattr;
    opers->mknod    = nufs_mknod;
    opers->rename   = nufs_rename;
    opers->chmod    = nufs_chmod;
    opers->utimens  = nufs_utimens;
    
    opers->open     = nufs_open;
    opers->link     = nufs_link;
    opers->unlink   = nufs_unlink;
    opers->read     = nufs_read;
    opers->write    = nufs_write;
    opers->truncate = nufs_truncate;
    
    opers->mkdir    = nufs_mkdir;
    opers->rmdir    = nufs_rmdir;
    opers->readdir  = nufs_readdir;
    
    opers->symlink  = nufs_symlink;
    opers->readlink = nufs_readlink;
}


// store FUSE operations defined in NUFS
struct fuse_operations fuse_opers;

/* Initialize FUSE-based filesystem NUFS */
int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    
    printf("\n\n====================== NUFS LOG ======================\n\n");
    
    // get data file path
    char* data_file = argv[--argc];
    
    // initialize superblock for NUFS in given data file
    printf("#-DISK: Mounting %s as data file\n", data_file);
    disk_mount(data_file);
    printf("@->: Success\n\n"); // log

    // initialize FUSE operations in NUFS
    nufs_init_fuse_opers(&fuse_opers);
    printf("#-NUFS: FUSE operations initialized\n\n");
    
    // call FUSE and give it struct with operations
    printf("#-NUFS: Calling FUSE to handle from here\n\n\n");
    return fuse_main(argc, argv, &fuse_opers, NULL);
}
