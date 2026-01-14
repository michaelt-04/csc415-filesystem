/**************************************************************
 * Class::  CSC-415-02 Spring 2025
 * Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
 * Student IDs:: 922525848, 922707016, 922711514, 918371654
 * GitHub-Name:: Jasuv
 * Group-Name:: Debug Thugs
 * Project:: Basic File System
 *
 * File:: mfs.h
 *
 * Description::
 *	This is the file system interface.
 *	This is the interface needed by the driver to interact with
 *	your filesystem.
 *
 **************************************************************/

#ifndef _MFS_H
#define _MFS_H
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "b_io.h"

#include <dirent.h>
#define FT_REGFILE DT_REG
#define FT_DIRECTORY DT_DIR
#define FT_LINK DT_LNK
#define BLOCK_SIZE 512

// Block the use of VCB block 0.
#define FS_RESERVED_BLOCK 1
#define FS_BLOCK_COUNT 40 // Reserve the use of first 40 blocks for freeSpaceMap
// 19,531 / 512 = 38 ish
#define FS_FIRST_USABLE_BLOCK (FS_RESERVED_BLOCK + FS_BLOCK_COUNT)

#define DIRECTORY_ENTRIES 32 // changed to 32
#define ENTRY_SIZE (DIRECTORY_ENTRIES * sizeof(de_struct))
// ensure de is exactly 1024 bytes or 2 blocks
#define MAX_DE_BLOCK_COUNT 182 // this is about 90 KB of data
#define BLOCKS_ALLOCATED_SIZE (MAX_DE_BLOCK_COUNT * sizeof(int))

#define LOCAL_PATH_MAX 256

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif

// This structure is returned by fs_readdir to provide the caller with information
// about each file as it iterates through a directory
struct fs_diriteminfo {
    unsigned short d_reclen; /* length of this record */
    unsigned char fileType;
    char d_name[256]; /* filename max filename is 255 characters */
};

/*
 * struct for the directory entries
 * defines both files and directories
 */
typedef struct de_struct {
    // char array to store name
    char file_name[256];
    // blob size in bytes
    size_t size;
    // entry type and permissions
    mode_t mode;
    // starting block number for blob data
    int blocks_allocated[MAX_DE_BLOCK_COUNT];
    // number of blocks allocated for blob data
    int blocks_count;
    // create date timestamp
    time_t date_created;
    // last modified timestamp
    time_t date_modified;
    // flag indicating blob is a directory
    int is_directory;
} de_struct;

// This is a private structure used only by fs_opendir, fs_readdir, and fs_closedir
// Think of this like a file descriptor but for a directory - one can only read
// from a directory.  This structure helps you (the file system) keep track of
// which directory entry you are currently processing so that everytime the caller
// calls the function readdir, you give the next entry in the directory
typedef struct
{
    /*****TO DO:  Fill in this structure with what your open/read directory needs  *****/
    unsigned short d_reclen;         /* length of this record */
    unsigned short dirEntryPosition; /* which directory entry position, like file pos */
    de_struct *directory;            /* Pointer to the loaded directory you want to iterate */
    struct fs_diriteminfo *di;       /* Pointer to the structure you return from read */
} fdDir;

// Key directory functions
int *newDir(de_struct *parentDir, mode_t mode);
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);
int fs_mv(const char *srcPath, const char *dstPath);

// Directory iteration functions
fdDir *fs_opendir(char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// Misc directory functions
char *fs_getcwd(char *pathname, size_t size);
int fs_setcwd(char *pathname); // linux chdir
int fs_isFile(char *filename); // return 1 if file, 0 otherwise
int fs_isDir(char *pathname);  // return 1 if directory, 0 otherwise
int fs_delete(char *filename); // removes a file
time_t getTime();              // returns the current time

typedef struct parseInfo {
    de_struct *parent;
    int index;
    char *lastElementName;
} parseInfo;

extern de_struct *rootDir;
extern de_struct *cwDir;
extern char *cwdName; // absolute path of cwdName?

int parsePath(char *pathName, parseInfo *ppi);

// helper functions used in parsePath
int findInDirectory(char *name, de_struct *parent);
int isDEaDir(de_struct *target); // returns 0 when it is a dir
de_struct *loadDirectory(de_struct *target);

// This is the strucutre that is filled in from a call to fs_stat
struct fs_stat {
    off_t st_size;        /* total size, in bytes */
    blksize_t st_blksize; /* blocksize for file system I/O */
    blkcnt_t st_blocks;   /* number of 512B blocks allocated */
    time_t st_accesstime; /* time of last access */
    time_t st_modtime;    /* time of last modification */
    time_t st_createtime; /* time of last status change */

    /* add additional attributes here for your file system */
};

int fs_stat(const char *path, struct fs_stat *buf);

// Added Structures (DE, VCB, etc.)

/*
 * struct for volume control blocks
 * metadata for a disk volume
 */
typedef struct vcb_struct {
    // char array for volume name
    char volume_name[64];
    // size of block in bytes
    size_t block_size;
    // total blocks in the volume
    int block_count;
    // starting block number for free space list
    int freespace_list_start;
    // starting block number for the root directory
    int root_dir_start;
    // signature to identify valid volume control block (up to 64 bits)
    long long signature;
} vcb_struct;

#endif
