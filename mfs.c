/**************************************************************
 * Class::  CSC-415-02 Spring 2025
 * Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
 * Student IDs:: 922525848, 922707016, 922711514, 918371654
 * GitHub-Name:: Jasuv
 * Group-Name:: Debug Thugs
 * Project:: Basic File System
 *
 * File:: mfs.c
 *
 * Description:: Handles all directory operations for the file system
 *
 *
 **************************************************************/

#include "mfs.h"
#include "freeSpace.h"
#include "fsLow.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

time_t getTime() {
    time_t now;
    return time(&now);
}

int *newDir(de_struct *parentDir, mode_t mode) {
    // no parent dir means initialize root dir
    int isRoot = (parentDir == NULL);

    // calculate the number of blocks for the directory
    // each de_struct is 1024 bytes which requires exactly 2 blocks
    // for 32 entries you need 64 blocks
    int blocksNeeded = (ENTRY_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE; // SHOULD BE 64
    // printf("blocksNeeded=%d should be 64\n", blocksNeeded);

    // allocate blocks for directory
    int *dirBlocks = allocateBlocks(blocksNeeded);
    if (dirBlocks == NULL) {
        printf("Error allocating blocks for new directory\n");
        return NULL;
    }

    // create directory de_struct array
    de_struct *dir = malloc(blocksNeeded * BLOCK_SIZE);
    if (dir == NULL) {
        printf("Error allocating buffer for new directory\n");
        return NULL;
    }
    memset(dir, 0, blocksNeeded * BLOCK_SIZE);

    // initialize empty entries
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        dir[i].file_name[0] = '\0';
        dir[i].size = 0;
        dir[i].mode = 0;
        memset(dir[i].blocks_allocated, 0, BLOCKS_ALLOCATED_SIZE);
        dir[i].blocks_count = 0;
        dir[i].date_created = 0;
        dir[i].date_modified = 0;
        dir[i].is_directory = 0;
    }

    // grab time
    time_t now = getTime();

    // initialize . entry
    strcpy(dir[0].file_name, ".");
    dir[0].file_name[255] = '\0';
    dir[0].size = ENTRY_SIZE;
    dir[0].mode = mode;
    memcpy(dir[0].blocks_allocated, dirBlocks, BLOCKS_ALLOCATED_SIZE);
    dir[0].blocks_count = blocksNeeded;
    dir[0].date_created = now;
    dir[0].date_modified = now;
    dir[0].is_directory = 1;

    // initialize .. entry
    strcpy(dir[1].file_name, "..");
    dir[1].file_name[255] = '\0';
    dir[1].size = ENTRY_SIZE;
    dir[1].mode = mode;
    if (isRoot) {
        memcpy(dir[1].blocks_allocated, dirBlocks, BLOCKS_ALLOCATED_SIZE);
    } else {
        memcpy(dir[1].blocks_allocated, parentDir[0].blocks_allocated, BLOCKS_ALLOCATED_SIZE);
    }
    dir[1].blocks_count = blocksNeeded;
    dir[1].date_created = now;
    dir[1].date_modified = now;
    dir[1].is_directory = 1;

    // write each block individually based on . blocks_allocated array
    for (int i = 0; i < blocksNeeded; i++) {
        void *dirToBlocks = (void *)((char *)dir + i * BLOCK_SIZE);
        if (LBAwrite(dirToBlocks, 1, dir[0].blocks_allocated[i]) != 1) {
            printf("Error writing block %d for new directory\n", i);
            free(dir);
            return NULL;
        }
    }

    if (isRoot)
        memcpy(rootDir, dir, ENTRY_SIZE);

    // cleanup
    free(dir);

    // return the blocks allocated for the new directory
    return dirBlocks;
}

int fs_mkdir(const char *pathname, mode_t mode) {
    // load parent directory
    de_struct *parentDir = NULL;
    parseInfo *ppi = malloc(sizeof(parseInfo));
    if (parsePath((char *)pathname, ppi) == -1) {
        printf("Error parsing parent directory\n");
        free(ppi);
        return -1;
    }
    parentDir = ppi->parent;

    // check if the directory already exists
    if (findInDirectory(ppi->lastElementName, parentDir) != -1) {
        printf("Error directory %s already exists at %s\n", ppi->lastElementName, pathname);
        free(ppi);
        return -1;
    }

    // create a new directory
    int *newDirBlocks = newDir(parentDir, mode);
    if (newDirBlocks == NULL) {
        printf("Error creating new directory\n");
        free(ppi);
        return -1;
    }

    // get current time
    time_t now = getTime();

    // add new directory entry to parent directory (if not root)
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        // find first blank directory entry
        if (parentDir[i].file_name[0] == '\0') {
            strcpy(parentDir[i].file_name, ppi->lastElementName);
            parentDir[i].file_name[255] = '\0'; // null terminator
            parentDir[i].size = ENTRY_SIZE;
            parentDir[i].mode = mode;
            int copySize = parentDir[0].blocks_count * sizeof(int);
            memcpy(parentDir[i].blocks_allocated, newDirBlocks, copySize);
            parentDir[i].blocks_count = DIRECTORY_ENTRIES * 2;
            parentDir[i].date_created = now;
            parentDir[i].date_modified = now;
            parentDir[i].is_directory = 1;

            // write the updated directory to disk
            for (int i = 0; i < parentDir[0].blocks_count; i++) {
                void *dirToBlocks = (void *)((char *)parentDir + i * BLOCK_SIZE);
                if (LBAwrite(dirToBlocks, 1, parentDir[0].blocks_allocated[i]) != 1) {
                    printf("Error writing updated block %d for parent directory\n", i);
                    if (parentDir != rootDir && parentDir != cwDir) {
                        free(parentDir);
                    }
                    return -1;
                }
            }
            return 0;
        }
    }

    printf("Error failed to find a blank directory entry in %s for %s\n", pathname, ppi->lastElementName);
    return -1;
}

int fs_rmdir(const char *pathname) {

    // cannot delete cwd
    if (strcmp(pathname, cwdName) == 0) {
        printf("Error cannot delete current working directory\n");
        return -1;
    }

    de_struct *rmdir = NULL;
    de_struct *parentDir = NULL;

    // load parent directory (current directory)
    parseInfo *ppi = malloc(sizeof(parseInfo));
    if (parsePath((char *)pathname, ppi) == -1) {
        printf("Error parsing parent directory in rmdir\n");
        free(ppi);
        return -1;
    }
    parentDir = ppi->parent;

    // check if the directory exists in the parent directory
    if (findInDirectory(ppi->lastElementName, parentDir) == -1) {
        printf("Error directory %s does not exist in parentDir\n", ppi->lastElementName);
        free(ppi);
        return -1;
    }

    // cannot delete cwd
    const char *slash = strrchr(cwdName, '/');                   // gets the last slash
    const char *pathEnd = (slash != NULL) ? slash + 1 : cwdName; // gets the last entry of the path
    if (strcmp(ppi->lastElementName, pathEnd) == 0) {
        printf("Error cannot delete current working directory\n");
        return -1;
    }

    // check if the directory entry is a direcotry
    if (parentDir[ppi->index].is_directory == 0) {
        printf("Error directory %s is not a directory\n", ppi->lastElementName);
        free(ppi);
        return -1;
    }

    // load the directory to remove
    rmdir = loadDirectory(&parentDir[ppi->index]);
    if (rmdir == NULL) {
        printf("Error loading directory from memory for removal %s\n", pathname);
        free(ppi);
        return -1;
    }

    // check if the directory is empty (only contains . and ..)
    // i am not sure if we need this check, this is just how linux does it
    int isEmpty = 1;
    for (int i = 2; i < DIRECTORY_ENTRIES; i++) {
        if (rmdir[i].file_name[0] != '\0') {
            isEmpty = 0;
            break;
        }
    }
    if (!isEmpty) {
        printf("Error directory %s is not empty\n", pathname);
        free(ppi);
        free(rmdir);
        return -1;
    }

    // free the blocks allocated to the directory entry in parent directory
    if (freeBlocks(parentDir[ppi->index].blocks_allocated, parentDir[ppi->index].blocks_count) == -1) {
        printf("Error freeing blocks for directory %s\n", pathname);
        free(ppi);
        free(rmdir);
        return -1;
    }

    // clear the entry data from the parent directory array
    parentDir[ppi->index].file_name[0] = '\0';
    parentDir[ppi->index].size = 0;
    parentDir[ppi->index].mode = 0;
    memset(parentDir[ppi->index].blocks_allocated, 0, BLOCKS_ALLOCATED_SIZE);
    parentDir[ppi->index].blocks_count = 0;
    parentDir[ppi->index].date_created = 0;
    parentDir[ppi->index].date_modified = 0;
    parentDir[ppi->index].is_directory = 0;

    // write the updated parent directory to disk
    for (int i = 0; i < parentDir[0].blocks_count; i++) {
        void *dirToBlocks = (void *)((char *)parentDir + i * BLOCK_SIZE);
        if (LBAwrite(dirToBlocks, 1, parentDir[0].blocks_allocated[i]) != 1) {
            printf("Error writing updated block %d for parent directory\n", i);
            return -1;
        }
    }

    // free the directory from memory
    free(rmdir);
    free(ppi);

    return 0;
}

int fs_mv(const char *srcPath, const char *dstPath) {
    de_struct *srcParent = NULL;
    de_struct *srcEntry = NULL;
    de_struct *dstParent = NULL;
    int dstIndex = -1;

    // parse the source path
    parseInfo *srcPpi = malloc(sizeof(parseInfo));
    // check if source is a full path or from current directory
    if (srcPath[0] == '/') {
        // printf("parsing %s\n", srcPath);
        if (parsePath((char *)srcPath, srcPpi) == -1) {
            printf("Error parsing source path %s\n", srcPath);
            free(srcPpi);
            return -1;
        }
    } else {
        // make a temp path/to/source
        char *cwSrcPath = malloc(strlen(cwdName) + strlen(srcPath) + 1);
        strcpy(cwSrcPath, cwdName);
        if (strcmp(cwdName, "/") != 0)
            strcat(cwSrcPath, "/"); // add a slash to cwd (if not root)
        strcat(cwSrcPath, srcPath);
        // printf("parsing %s\n", cwSrcPath);
        if (parsePath((char *)cwSrcPath, srcPpi) == -1) {
            printf("Error parsing source path %s\n", cwSrcPath);
            free(cwSrcPath);
            free(srcPpi);
            return -1;
        }
        free(cwSrcPath);
    }

    // get the source directory entry in source directory
    srcParent = srcPpi->parent;
    if (srcParent == NULL)
        printf("Error the source has no parent directory\n");
    srcEntry = &srcParent[srcPpi->index];

    // parse the destination path
    parseInfo *dstPpi = malloc(sizeof(parseInfo));
    // check if file name is in path (add it if not)
    const char *slash = strrchr(dstPath, '/');                   // gets the last slash
    const char *pathEnd = (slash != NULL) ? slash + 1 : dstPath; // gets the last entry of the path
    if (strcmp(pathEnd, srcEntry->file_name) == 0) {
        // printf("parsing %s\n", dstPath);
        if (parsePath((char *)dstPath, dstPpi) == -1) {
            printf("Error parsing destination path %s\n", dstPath);
            free(srcPpi);
            free(dstPpi);
            return -1;
        }
    } else {
        // make a temp path/to/destination/source_file
        char *fullDstPath = malloc(strlen(dstPath) + strlen(srcEntry->file_name) + 1);
        strcpy(fullDstPath, dstPath);
        // add a slash if not root and if dstPath doesnt already have one
        if (strcmp(dstPath, "/") != 0 && dstPath[strlen(dstPath) - 1] != '/')
            strcat(fullDstPath, "/");
        strcat(fullDstPath, srcEntry->file_name);
        // printf("parsing %s\n", fullDstPath);
        if (parsePath((char *)fullDstPath, dstPpi) == -1) {
            printf("Error parsing destination path %s\n", fullDstPath);
            free(fullDstPath);
            free(srcPpi);
            free(dstPpi);
            return -1;
        }
        free(fullDstPath);
    }
    dstParent = dstPpi->parent;
    if (dstParent == NULL)
        printf("Error the destination has no parent directory\n");

    // check if there is space in the destination directory
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        if (dstParent[i].file_name[0] == '\0') {
            dstIndex = i;
            break;
        }
    }
    if (dstIndex == -1) {
        printf("Error destination directory %s is full\n", dstPath);
        free(srcPpi);
        free(dstPpi);
        return -1;
    }

    // printf("writing %s to %d in parent dir\n", srcEntry->file_name, dstIndex);

    // move the source entry to the destination directory
    strcpy(dstParent[dstIndex].file_name, srcEntry->file_name);
    dstParent[dstIndex].size = srcEntry->size;
    dstParent[dstIndex].mode = srcEntry->mode;
    memcpy(dstParent[dstIndex].blocks_allocated, srcEntry->blocks_allocated, BLOCKS_ALLOCATED_SIZE);
    dstParent[dstIndex].blocks_count = srcEntry->blocks_count;
    dstParent[dstIndex].date_created = srcEntry->date_created;
    dstParent[dstIndex].date_modified = getTime();
    dstParent[dstIndex].is_directory = srcEntry->is_directory;

    // clear the source entry in the source parent directory
    srcEntry->file_name[0] = '\0';
    srcEntry->size = 0;
    srcEntry->mode = 0;
    memset(srcEntry->blocks_allocated, 0, BLOCKS_ALLOCATED_SIZE);
    srcEntry->blocks_count = 0;
    srcEntry->date_created = 0;
    srcEntry->date_modified = 0;
    srcEntry->is_directory = 0;

    // write the updated source parent directory to disk
    for (int i = 0; i < srcParent[0].blocks_count; i++) {
        void *dirToBlocks = (void *)((char *)srcParent + i * BLOCK_SIZE);
        if (LBAwrite(dirToBlocks, 1, srcParent[0].blocks_allocated[i]) != 1) {
            printf("Error writing updated block %d for source parent directory\n", i);
            free(srcPpi);
            free(dstPpi);
            return -1;
        }
    }

    // write the updated destination parent directory to disk
    for (int i = 0; i < dstParent[0].blocks_count; i++) {
        void *dirToBlocks = (void *)((char *)dstParent + i * BLOCK_SIZE);
        if (LBAwrite(dirToBlocks, 1, dstParent[0].blocks_allocated[i]) != 1) {
            printf("Error writing updated block %d for destination parent directory\n", i);
            free(srcPpi);
            free(dstPpi);
            return -1;
        }
    }

    // cleanup
    free(srcPpi);
    free(dstPpi);

    return 0;
}

// Directory iteration functions
fdDir *fs_opendir(char *pathname) {

    parseInfo *ppi = malloc(sizeof(parseInfo));
    if (ppi == NULL) {
        printf("Error mallocing ppi\n");
        return NULL;
    }

    int check = parsePath(pathname, ppi);

    if (check != 0) {
        return NULL;
    }

    fdDir *fd = malloc(sizeof(fdDir));
    if (fd == NULL) {
        printf("Error mallocing fd\n");
        free(ppi);
        return NULL;
    }

    // initialize fd
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        fd->d_reclen += ppi->parent[i].size;
    }

    fd->dirEntryPosition = 0;

    if (ppi->index == -2) {
        fd->directory = ppi->parent;
    } else {
        fd->directory = loadDirectory(&ppi->parent[ppi->index]);
        if (fd->directory == NULL) {
            printf("Error  loading fd->directory\n");
            free(ppi);
            return NULL;
        }
    }

    fd->di = malloc(sizeof(struct fs_diriteminfo));
    if (fd->di == NULL) {
        printf("Error mallocing fd->di\n");
        free(ppi);
        free(fd);
        return NULL;
    }

    return fd;
}

struct fs_diriteminfo *fs_readdir(fdDir *dirp) {

    // skip any invalid entries
    while (dirp->dirEntryPosition < DIRECTORY_ENTRIES && dirp->directory[dirp->dirEntryPosition].file_name[0] == '\0') {
        dirp->dirEntryPosition++;
    }

    if (dirp->dirEntryPosition >= DIRECTORY_ENTRIES) {
        return NULL;
    }

    de_struct *de = &dirp->directory[dirp->dirEntryPosition];

    // initialize diriteminfo
    dirp->di->d_reclen = de->size;
    strncpy(dirp->di->d_name, de->file_name, 255);
    dirp->di->d_name[255] = '\0';

    if (de->is_directory) {
        dirp->di->fileType = 'd'; // use FT_DIRECTORY and FT_REGFILE?
    } else {
        dirp->di->fileType = '-';
    }

    // move to next entry for next call
    dirp->dirEntryPosition++;

    return dirp->di;
}

int fs_closedir(fdDir *dirp) {
    if (dirp == NULL) {
        return -1;
    }

    if (dirp->directory != NULL && dirp->directory != rootDir && dirp->directory != cwDir) {
        free(dirp->directory);
        dirp->directory = NULL;
    }

    if (dirp->di != NULL) {
        free(dirp->di);
    }

    free(dirp);
    return 0;
}

// Misc directory functions
char *fs_getcwd(char *pathname, size_t size) {
    // Handle incorrect requests
    if (cwdName == NULL || pathname == NULL || size == 0) {
        return NULL;
    }

    strncpy(pathname, cwdName, size - 1);
    // Force null terminate the EOL.
    pathname[size - 1] = '\0';

    return pathname;
}

// linux chdir
int fs_setcwd(char *pathname) {
    // printf("setcwd pathname=%s\n", pathname);

    // Handle failures or edge cases.
    if (pathname == NULL || strlen(pathname) == 0) {
        printf("[fs_setcwd] Error: Invalid file path: %s", pathname);
        return -1;
    }

    // Special case for root directory
    if (strcmp(pathname, "/") == 0) {
        cwDir = rootDir;
        strcpy(cwdName, "/");
        // printf("[fs_setcwd] Changed to root directory\n");
        return 0;
    }

    // Save the original directory to free at the end
    // This is needed to not hit double frees and segfaults when cding
    de_struct *dirToFree = NULL;
    if (cwDir != rootDir) {
        dirToFree = cwDir;
    }

    // Handle path with multiple subdirectories.
    if (strchr(pathname, '/') != NULL && pathname[0] != '/') {

        // Call parsePath to resolve requested dir.
        // create a copy of pathname so parsePath cannot modify it
        char pathCopy[LOCAL_PATH_MAX];
        strncpy(pathCopy, pathname, LOCAL_PATH_MAX - 1);
        pathCopy[LOCAL_PATH_MAX - 1] = '\0';

        char *saveptr;
        // Tokenize the path. attempt to check each part individually
        char *part = strtok_r(pathCopy, "/", &saveptr);

        // Process each path component
        while (part != NULL) {
            // If not able to set setcwd to the part return -1 and set 0 until we have a valid part.
            if (fs_setcwd(part) != 0) {
                return -1;
            }
            part = strtok_r(NULL, "/", &saveptr);
        }

        return 0;
    }

    else if (pathname[0] == '/') {
        // Absolute path - start at root
        cwDir = rootDir;
        strcpy(cwdName, "/");

        // Process the rest of the path if any
        if (strlen(pathname) > 1) {
            return fs_setcwd(pathname + 1);
        }
        return 0;
    }

    // Handle ".." special case - parent directory
    if (strcmp(pathname, "..") == 0) {
        // Update path first
        char *lastSlash = strrchr(cwdName, '/');
        if (lastSlash == cwdName) {
            // Already at root
            cwDir = rootDir;
            strcpy(cwdName, "/");
            return 0;
        }
        *lastSlash = '\0';

        // If we ended up with empty string, set to root
        if (strlen(cwdName) == 0) {
            strcpy(cwdName, "/");
        }

        // Find the parent directory
        parseInfo *ppi = malloc(sizeof(parseInfo));
        if (ppi == NULL) {
            printf("[fs_setcwd] Error: malloc failed\n");
            return -1;
        }

        char pathCopy[LOCAL_PATH_MAX];
        strncpy(pathCopy, cwdName, LOCAL_PATH_MAX - 1);
        pathCopy[LOCAL_PATH_MAX - 1] = '\0';

        // Validate result.
        int result = parsePath(pathCopy, ppi);

        // Special case for root
        if (strcmp(cwdName, "/") == 0) {
            cwDir = rootDir;
            free(ppi);
            ppi = NULL;
            return 0;
        }

        // We were not able to find anything.
        if (result != 0) {
            printf("[fs_setcwd] Error: Could not resolve path: %s\n", cwdName);
            free(ppi);
            return -1;
        }

        // Load the parent directory
        if (ppi->index == -2) {
            // This means the target is root directory.
            cwDir = rootDir;
        } else {
            // free cwdir before loading it again
            free(cwDir);
            de_struct *newDir = loadDirectory(&ppi->parent[ppi->index]);
            if (newDir == NULL) {
                printf("[fs_setcwd] Error: Failed to load directory\n");
                free(ppi);
                return -1;
            }
            cwDir = newDir;
        }

        // Clean up parseInfo but not the parent directory
        free(ppi);

        // printf("[fs_setcwd] Changed to directory: %s\n", cwDir[0].file_name);
        // printf("[fs_setcwd] cwdName updated to: %s\n", cwdName);

        // Free the original directory saved at the beginning if needed
        if (dirToFree != NULL && dirToFree != cwDir && dirToFree != rootDir) {
            free(dirToFree);
        }

        return 0;
    }

    // Handle the regular case - single directory
    parseInfo *ppi = malloc(sizeof(parseInfo));
    if (ppi == NULL) {
        printf("[fs_setcwd] Error: malloc failed\n");
        return -1;
    }

    char pathCopy[LOCAL_PATH_MAX];
    strncpy(pathCopy, pathname, LOCAL_PATH_MAX - 1);
    pathCopy[LOCAL_PATH_MAX - 1] = '\0';

    int result = parsePath(pathCopy, ppi);

    if (result != 0 || ppi->index == -1 || ppi->parent == NULL) {
        printf("[fs_setcwd] Error: Could not resolve path: %s\n", pathname);
        free(ppi);
        ppi = NULL;
        return -1;
    }

    // Handle special case for root directory
    if (ppi->index == -2) {
        cwDir = rootDir;
        strcpy(cwdName, "/");
        // printf("[fs_setcwd] Changed to root directory\n");
    }

    else {
        // Check if it's a directory
        if (!ppi->parent[ppi->index].is_directory) {
            printf("[fs_setcwd] Error: %s is not a directory\n", pathname);
            free(ppi);
            ppi = NULL;
            return -1;
        }

        // Load the directory
        de_struct *newDir = loadDirectory(&ppi->parent[ppi->index]);
        if (newDir == NULL) {
            printf("[fs_setcwd] Error: Failed to load directory\n");
            free(ppi);
            ppi = NULL;
            return -1;
        }

        // Update current directory
        cwDir = newDir;

        // Update path
        if (strcmp(pathname, ".") != 0) {
            if (strcmp(cwdName, "/") != 0) {
                strcat(cwdName, "/");
            }
            strcat(cwdName, pathname);
        }

        // printf("[fs_setcwd] Changed to directory: %s\n", cwDir[0].file_name);
    }

    // printf("[fs_setcwd] cwdName updated to: %s\n", cwdName);

    // Free the parseInfo but not the parent
    free(ppi);
    ppi = NULL;

    // Free the original directory saved at the beginning if needed
    if (dirToFree != NULL && dirToFree != cwDir && dirToFree != rootDir) {
        free(dirToFree);
    }

    return 0;
}

// return 1 if file, 0 otherwise

int fs_isFile(char *filename) {

    // Check if file exists
    if (filename == NULL) {
        return 0;
    }

    // create a temp path for parsePath
    char *tmpPath = malloc(strlen(filename));
    if (tmpPath == NULL) {
        printf("Error mallocing space for temp pathname\n");
        return -1;
    }
    strcpy(tmpPath, filename);

    // Allocate memory for parseInfo
    parseInfo *ppi = malloc(sizeof(parseInfo));

    if (ppi == NULL) {
        free(tmpPath);
        return -1;
    }

    // Use parsePath to find the file
    int result = parsePath(tmpPath, ppi);

    // If pasePath fails or the file wasnt found
    if (result != 0 || ppi->index < 0) {
        // It is not a file
        free(tmpPath);
        free(ppi);
        return 0;
    }

    // Access directory entry
    de_struct *entry;
    entry = &ppi->parent[ppi->index];

    // If the entry is a directory
    if (entry->is_directory) {
        free(tmpPath);
        if (ppi->parent != rootDir && ppi->parent != cwDir) {
            free(ppi->parent);
        }
        free(ppi);
        return 0;
    }

    // It is a file
    free(tmpPath);
    if (ppi->parent != rootDir && ppi->parent != cwDir) {
        free(ppi->parent);
    }
    free(ppi);
    return 1;
}

// return 1 if directory, 0 otherwise

int fs_isDir(char *pathname) {

    // create a temp path for parsePath
    char *tmpPath = malloc(strlen(pathname));
    if (tmpPath == NULL) {
        printf("Error mallocing space for temp pathname\n");
        return -1;
    }
    strcpy(tmpPath, pathname);

    // Allocate memory for parseInfo
    parseInfo *ppi = malloc(sizeof(parseInfo));

    // Use parsePath to find the file
    // int result = parsePath(pathname, ppi);
    int result = parsePath(tmpPath, ppi);

    // If pasePath fails or the file wasnt found
    if (result != 0) {
        // It is not a directory
        free(tmpPath);
        free(ppi);
        return 0;
    }

    de_struct *entry;
    entry = &ppi->parent[ppi->index];

    // If the entry is a directory
    if (entry->is_directory) {
        free(tmpPath);
        free(ppi);
        return 1;
    }

    // It is a file
    free(tmpPath);
    free(ppi);
    return 0;
}

// removes a file
int fs_delete(char *filename) {

    // Check for calid input
    if (filename == NULL) {
        return -1;
    }

    // Use parsePath to locate the file
    parseInfo *ppi = malloc(sizeof(parseInfo));
    if (ppi == NULL) {
        return -1;
    }

    int result = parsePath(filename, ppi);

    // Check if the file was found
    if (result != 0 || ppi->index < 0) {
        free(ppi);
        return -1;
    }

    // Get the file entry from the parent directory
    de_struct *entry = &ppi->parent[ppi->index];

    // Free allocated blocks
    for (int i = 0; i < entry->blocks_count; i++) {
        freeBlocks(entry->blocks_allocated, entry->blocks_count);
    }

    // Clear the entry to mark as deleted
    entry->file_name[0] = '\0';
    entry->size = 0;
    entry->blocks_count = 0;
    entry->date_created = 0;
    entry->date_modified = 0;
    entry->is_directory = 0;

    // Calculate how many blocks the directory uses
    int dirBlocks = (ENTRY_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Write parent dir back to disk
    int dirStartBlock = ppi->parent[0].blocks_allocated[0];
    LBAwrite(ppi->parent, dirBlocks, dirStartBlock);

    free(ppi);
    return 0;
}

int fs_stat(const char *path, struct fs_stat *buf) {
    // Check for valid input
    if (path == NULL || buf == NULL) {
        return -1;
    }

    // Allocate memory for parseinfo
    parseInfo *ppi = malloc(sizeof(parseInfo));

    // Resolve the path to a directory entry
    int result = parsePath((char *)path, ppi);
    if (result != 0 || ppi->index < 0) {
        free(ppi);
        return -1;
    }

    // Get the entry
    de_struct *entry = &ppi->parent[ppi->index];

    // Fill in the stat struct
    buf->st_size = entry->size;
    buf->st_blksize = BLOCK_SIZE;
    buf->st_blocks = entry->blocks_count * (BLOCK_SIZE / 512);
    buf->st_createtime = entry->date_created;
    buf->st_modtime = entry->date_modified;
    buf->st_accesstime = entry->date_modified;

    free(ppi);
    return 0;
}

int parsePath(char *pathName, parseInfo *ppi) {
    de_struct *parent;
    de_struct *startParent;
    char *savePtr;
    char *token1;
    char *token2;

    if (pathName == NULL) {
        printf("Error invalid pathname to parse\n");
        return -1;
    }

    if (pathName[0] == '/') {
        startParent = rootDir;
    } else {
        startParent = cwDir;
    }

    parent = startParent;

    token1 = strtok_r(pathName, "/", &savePtr);

    if (token1 == NULL) {
        if (pathName[0] == '/') {
            ppi->parent = parent;
            ppi->index = -2;
            ppi->lastElementName = NULL;
            return 0;
        } else {
            return -1;
        }
    }

    while (1) {
        int idx = findInDirectory(token1, parent);
        token2 = strtok_r(NULL, "/", &savePtr);

        if (token2 == NULL) {
            ppi->parent = parent;
            ppi->index = idx;
            ppi->lastElementName = token1;
            return 0;
        } else {
            if (idx == -1) {
                return -2;
            }

            if (!isDEaDir(&parent[idx]))
                return -1;

            de_struct *tempParent = loadDirectory(&parent[idx]);
            if (tempParent == NULL) {
                return -1;
            }

            if (parent != startParent && parent != rootDir && parent != cwDir) {
                // free(parent);
            }
            if (tempParent[0].blocks_allocated[0] == rootDir[0].blocks_allocated[0]) {
                parent = rootDir;
            } else {
                parent = tempParent;
            }
            token1 = token2;
        }
    }
}

int findInDirectory(char *name, de_struct *parent) {
    if (name == NULL || parent == NULL) {
        return -1;
    }

    for (int i = 0; i < DIRECTORY_ENTRIES; i++) { // use DIRECTORY_ENTRIES for this?
        // Skip unused and empty entries when finding.
        if (parent[i].file_name[0] == '\0')
            continue;

        if (strcmp(name, parent[i].file_name) == 0) {
            return i; // returns the location of a file/dir in the directory
        }
    }

    return -1;
}

int isDEaDir(de_struct *target) {
    if (target->is_directory) { // might need to change
        return 1;
    }

    return 0;
}

de_struct *loadDirectory(de_struct *target) {
    // check if target is NULL or not a directory
    if (target == NULL || !target->is_directory) {
        return NULL;
    }

    // allocate memory for the directory entries
    de_struct *entries = malloc(target->blocks_count * BLOCK_SIZE);
    if (entries == NULL) {
        return NULL;
    }
    // Clean the memory before loadDirectory
    memset(entries, 0, target->blocks_count * BLOCK_SIZE);

    // read each block individually based on the blocks_allocated array
    for (int i = 0; i < target->blocks_count; i++) {
        void *blockData = (void *)((char *)entries + i * BLOCK_SIZE);
        if (LBAread(blockData, 1, target->blocks_allocated[i]) != 1) {
            printf("Error reading block %d for directory\n", i);
            free(entries);
            return NULL;
        }
    }

    // printf("[loadDirectory] Read success. First entry name: '%s'\n", entries[0].file_name);
    // printf("[loadDirectory] Second entry name: '%s'\n", entries[1].file_name);
    // printf("[loadDirectory] blocks_allocated count: '%d'\n", entries[0].blocks_count);

    return entries;
}
