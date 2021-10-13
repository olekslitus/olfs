# OLFS 
- oleks file system

FUSE filesystem driver that will let you mount a 1MB disk image (data file) as a filesystem.

You’ll need to install the `fuse` and `libfuse-dev` packages. Make sure your working directory is a proper Linux filesystem, not a remote-mounted Windows or Mac directory.

### Functionality
- [x] Create files.
- [x] List the files in the filesystem root directory (where you mounted it).
- [x] Write to small files (under 4k).
- [x] Read from small files (under 4k).
- [x] Delete files.
- [x] Rename files.
- [x] Read and write from files larger than one block. For example, you should be able to support one hundred 1k files or five 100k files.
- [x] Create directories and nested directories. Directory depth should only be limited by disk space (and possibly the POSIX API).
- [x] Remove directories.
- [x] Hard links.
- [x] Symlinks
- [x] Support modification and display of metadata (permissions and timestamps) for files and directories.
- [x] Don't worry about multiple users. Assume the user mounting the filesystem is also the owner of any filesystem objects.