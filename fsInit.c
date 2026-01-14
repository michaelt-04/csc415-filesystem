/**************************************************************
 * Class::  CSC-415-02 Spring 2025
 * Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
 * Student IDs:: 922525848, 922707016, 922711514, 918371654
 * GitHub-Name:: Jasuv
 * Group-Name:: Debug Thugs
 * Project:: Basic File System
 *
 * File:: fsInit.c
 *
 * Description:: Main driver for file system assignment.
 *
 * This file is where you will start and initialize your system
 *
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "freeSpace.h"
#include "fsLow.h"
#include "mfs.h"

#define VCB_SIGNATURE 0x4275675468756773 // "BugThugs" signature

int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize) {
    printf("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
    /* TODO: Add any code you need to initialize your file system. */

    vcb_struct *vcb = malloc(blockSize);
    if (vcb == NULL) {
        printf("Failed to malloc for vcb!\n");
        return -1;
    }

    int read = LBAread(vcb, 1, 0);
    if (read != 1) {
        printf("LBAread error for vcb!\n");
        free(vcb);
        vcb = NULL;
        return -1;
    }

    int blocksNeeded = (ENTRY_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (rootDir == NULL) {
        rootDir = malloc(blocksNeeded * BLOCK_SIZE);
        if (rootDir == NULL) {
            printf("Error mallocing rootDir\n");
            free(vcb);
            vcb = NULL;
            return -1;
        }
    }

    if (cwdName == NULL) {
        cwdName = malloc(LOCAL_PATH_MAX);
        if (cwdName == NULL) {
            printf("Error mallocing cwdName\n");
            free(rootDir);
            rootDir = NULL;
            free(vcb);
            vcb = NULL;
            return -1;
        }
        strcpy(cwdName, "/");
    }

    if (vcb->signature == VCB_SIGNATURE) {
        printf("Volume is already formatted!\n");

        // Attempt to load the FreeSpaceMap via loadFreeSpaceMap() from freeSpace.c when we are re-reading the disk.
        if (loadFreeSpaceMap(blockSize, vcb->freespace_list_start, vcb->block_count) != 0) {
            printf("Failed to load freeSpaceMap from disk!\n");
            free(vcb);
            vcb = NULL;
            free(rootDir);
            rootDir = NULL;
            return -1;
        }

        int rootBlock = vcb->root_dir_start;

        int check = LBAread(rootDir, blocksNeeded, rootBlock);
        if (check != blocksNeeded) {
            printf("LBAread error for rootDir\n");
            free(vcb);
            vcb = NULL;
            free(rootDir);
            rootDir = NULL;
            return -1;
        }

        /*
        printf("----- PRINTING ROOTDIR -----\n");

        for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
            if (rootDir[i].file_name[0] != '\0') {
                printf("entry[%d] in rootDir: %s %d\n", i, rootDir[i].file_name, rootDir[i].is_directory);

                for (int k = 0; k < DIRECTORY_ENTRIES; k++) {
                    if (rootDir[k].file_name[0] != '\0') {
                        printf("\tentry[i].blocks_allocated[%d]: %d\n", k, rootDir[i].blocks_allocated[k]);
                    }
                }
            }
        }

        printf("----- END PRINTING -----\n");
        */

        cwDir = rootDir;

        printf("Root directory loaded from disk!\n");

        free(vcb);
        vcb = NULL;
        return 0;
    }

    else {

        // Format volume now

        memset(vcb, 0, blockSize);                     // clear vcb before initializing
        strcpy(vcb->volume_name, "DebugThugs Volume"); // set name to something else
        vcb->block_size = blockSize;
        vcb->block_count = numberOfBlocks;
        vcb->signature = VCB_SIGNATURE;
        // Passing the first allocated free block to the freespace_list_start
        vcb->freespace_list_start = FS_RESERVED_BLOCK;

        // Call Free Space initialization and raise a failure if we cant.
        int isInitFreeSpace = initFreeSpace(numberOfBlocks, blockSize);

        if (isInitFreeSpace != 0) {
            printf("Error InitFreeSpace Failed!\n");

            if (cwDir == rootDir) {
                cwDir = NULL;
            }
            free(rootDir);
            rootDir = NULL;
            free(vcb);
            vcb = NULL;
            return -1;
        }
    }

    /* TODO INIT ROOT DIR */

    // get root dir block without allocating too few memory.
    // And wipe it clean for alll blocks
    blocksNeeded = (ENTRY_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    rootDir = malloc(blocksNeeded * BLOCK_SIZE);
    memset(rootDir, 0, blocksNeeded * BLOCK_SIZE);

    // newDir->allocateBlocks() needs freeing
    int *rootBlocks = newDir(NULL, 0755);
    if (rootBlocks[0] == -1) {
        printf("Failed to init root dir\n");
        free(rootDir);
        rootDir = NULL;
        free(vcb);
        vcb = NULL;
        return -1;
    }
    // set new block as root dir start in vcb
    vcb->root_dir_start = rootBlocks[0];
    cwDir = rootDir;

    int write = LBAwrite(vcb, 1, 0);
    if (write != 1) {
        printf("LBAwrite error for vcb!\n");
        if (cwDir == rootDir) {
            cwDir = NULL;
        }
        free(rootDir);
        rootDir = NULL;
        free(vcb);
        vcb = NULL;
        return -1;
    }

    printf("Volume formatted!\n");

    // free only if its not NULL otherwise the system gets segfault.
    if (rootBlocks != NULL) {
        free(rootBlocks);
    }
    free(vcb);
    vcb = NULL;
    return 0;
}

void exitFileSystem() {
    printf("System exiting\n");

    if (rootDir != NULL) {
        if (cwDir == rootDir) {
            cwDir = NULL;
        }
        free(rootDir);
        rootDir = NULL;
    }

    if (cwDir != NULL && cwDir != rootDir) {
        free(cwDir);
        cwDir = NULL;
    }

    if (cwdName != NULL) {
        free(cwdName);
        cwdName = NULL;
    }

    if (freeSpaceMap != NULL) {
        free(freeSpaceMap);
        freeSpaceMap = NULL;
    }
}
