/**************************************************************
 * Class::  CSC-415-02 Spring 2025
 * Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
 * Student IDs:: 922525848, 922707016, 922711514, 918371654
 * GitHub-Name:: Jasuv
 * Group-Name:: Debug Thugs
 * Project:: Basic File System
 *
 * File:: freeSpace.c
 *
 * Description::
 *   Initializing and allocating free space
 *
 **************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freeSpace.h"
#include "fsLow.h"
#include "mfs.h"

// Other folks should just refer to this.
char *freeSpaceMap = NULL;
// Holds the total managed free space size.
int freeSpaceMapSize = 0;

// Initialize free space
int initFreeSpace(int blockCount, int sizeOfBlock) {
    // Allocate enough space for ALL blocks in the volume, not just system blocks
    int bitmapBytes = blockCount * sizeof(char);
    int blocksToWrite = (bitmapBytes + sizeOfBlock - 1) / sizeOfBlock;

    // Malloc the space needed for freeSpaceMap
    freeSpaceMap = (char *)malloc(blocksToWrite * sizeOfBlock);
    freeSpaceMapSize = blockCount;

    if (freeSpaceMap == NULL) {
        printf("Error allocating free space map\n");
        return -1;
    }

    // Zero out the entire bitmap
    memset(freeSpaceMap, 0, blocksToWrite * sizeOfBlock);

    for (int i = 0; i < FS_FIRST_USABLE_BLOCK; i++) {
        freeSpaceMap[i] = 1;
    }

    // Calculate how many blocks we need to write to disk
    int blocksNeededForBitmap = (bitmapBytes + sizeOfBlock - 1) / sizeOfBlock;

    // Make sure FS_BLOCK_COUNT is sufficient to track all blocks.
    if (blocksNeededForBitmap > FS_BLOCK_COUNT) {
        printf("Warning: FS_BLOCK_COUNT (%d) is smaller than needed (%d) for the bitmap\n",
               FS_BLOCK_COUNT, blocksNeededForBitmap);
    }

    // Write the bitmap to disk
    int writtenDataToDisk = LBAwrite(freeSpaceMap, blocksToWrite, FS_RESERVED_BLOCK);
    if (writtenDataToDisk != blocksToWrite) {
        printf("Error writing free space map to disk\n");
        free(freeSpaceMap);
        freeSpaceMap = NULL;
        freeSpaceMapSize = 0;
        return -1;
    }

    return 0;
}

int *allocateBlocks(int count) {
    if (count <= 0 || freeSpaceMap == NULL || freeSpaceMapSize == 0) {
        printf("Invalid Free Space allocation request, or uninitialized free space\n");
        return NULL;
    }

    // Set allocatedBlocks to 512 bytes and wipe it clean to avoid out of bounds memory
    int *allocatedBlocks = malloc(BLOCKS_ALLOCATED_SIZE);
    memset(allocatedBlocks, 0, BLOCKS_ALLOCATED_SIZE);
    if (allocatedBlocks == NULL) {
        printf("Memory allocation failed on allocatedBlock\n");
        return NULL;
    }

    // Allocate free block as used once we have conditions are met.
    int availableBlockCount = 0;
    for (int i = FS_FIRST_USABLE_BLOCK; i < freeSpaceMapSize && availableBlockCount < count; i++) {
        // If free mark it used.
        if (freeSpaceMap[i] == 0) {
            freeSpaceMap[i] = 1;
            // Update allocatedBlocks index.
            allocatedBlocks[availableBlockCount++] = i;
        }
    }

    // If for whatever reason, we would not have enough blocks
    // We have to roll it back
    if (availableBlockCount < count) {
        printf("Error finding available free space.");
        for (int i = 0; i < availableBlockCount; i++) {
            freeSpaceMap[allocatedBlocks[i]] = 0;
        }

        free(allocatedBlocks);
        return NULL;
    }
    // Write the freeSpaceMap to disk and make sure all blocks are written.
    // If LBAWrite doesnt match what we expect, treat it as a failure.
    int blocksToWrite = (freeSpaceMapSize * sizeof(char) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int written = LBAwrite(freeSpaceMap, blocksToWrite, FS_RESERVED_BLOCK);
    if (written != blocksToWrite) {
        printf("Error! Failed to write updated free space map to the disk after allocation\n");
        for (int i = 0; i < count; i++) {
            freeSpaceMap[allocatedBlocks[i]] = 0;
        }

        free(allocatedBlocks);
        return NULL;
    }

    return allocatedBlocks;
}

int freeBlocks(int *blockArray, int count) {
    int blockIndex = 0; // initialize variable outside loop
    for (int i = 0; i < count; i++) {
        blockIndex = blockArray[i];
        if (blockIndex <= 0 || blockIndex >= freeSpaceMapSize)
            continue;

        // Handle incorrect cases and protect FS reserved blocks.
        if (freeSpaceMap == NULL || blockIndex < 0 || blockIndex >= freeSpaceMapSize) {
            printf("Error: Invalid block number or uninitalized free space map!");
            return -1;
        }

        if (blockIndex < FS_RESERVED_BLOCK + FS_BLOCK_COUNT) {
            printf("Error: Block %d is assigned to File System. In order to free, please format the disk instead.", blockIndex);
            return -1;
        }

        if (freeSpaceMap[blockIndex] == 0) {
            printf("Error: Block %d is already free.\n", blockIndex);
            return -1;
        }

        // Don't trust the system, set everything to zero.
        uint8_t buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        int erasedBlock = LBAwrite(buffer, 1, blockIndex);
        if (!erasedBlock) {
            printf("Error: unable to erase the block contents of block %d\n", blockIndex);
            return -1;
        }

        // Set the block free.
        freeSpaceMap[blockIndex] = 0;
    }

    int blocksToWrite = (freeSpaceMapSize * sizeof(char) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int writtenDataToDisk = LBAwrite(freeSpaceMap, blocksToWrite, FS_RESERVED_BLOCK);
    if (writtenDataToDisk != blocksToWrite) {
        printf("Error writing updated freeSpaceMap to disk!\n");
        return -1;
    }

    return 0;
}

int checkBlockAvailability(int blockIndex) {
    // Handle incorrect cases.
    if (freeSpaceMap == NULL || blockIndex < 0 || blockIndex >= freeSpaceMapSize) {
        printf("Error: Invalid block number or uninitalized free space map!");
        return -1;
    }

    // If the system block is requested, return 1 to highlight system block.
    // This would prevent any accidental allocation in another layer.
    if (blockIndex < FS_RESERVED_BLOCK + FS_BLOCK_COUNT) {
        return 1;
    }

    // 0 = used, 1 = free
    if (freeSpaceMap[blockIndex] == 0) {
        return 1;
    }

    else {
        return 0;
    }
}

// Function to load FreeSpaceMap while initialization.
int loadFreeSpaceMap(int blockSize, int startBlock, int totalBlockCount) {
    if (freeSpaceMap != NULL) {
        // just in case it's already allocated
        free(freeSpaceMap);
    }

    // Set global variable freeSpaceMapSize so other functions track totalBlockCount appropriately.
    freeSpaceMapSize = totalBlockCount;

    // Calculate critical freeSpaceMapSize and recover the state.
    int mapSizeBytes = (freeSpaceMapSize * sizeof(char));
    int blocksToRead = (mapSizeBytes + blockSize - 1) / blockSize;

    // Malloc space to hold all freeSpaceMap sectors.
    freeSpaceMap = malloc(blocksToRead * blockSize);
    if (freeSpaceMap == NULL) {
        printf("[FreeSpaceLoader] Failed to allocate memory for reading freeSpaceMap!\n");
        return -1;
    }

    // Check given blocks and attempt to read them
    int check = LBAread(freeSpaceMap, blocksToRead, startBlock);
    if (check != blocksToRead) {
        printf("[FreeSpaceLoader] LBAread failed for freeSpaceMap!\n");
        free(freeSpaceMap);
        freeSpaceMap = NULL;
        return -1;
    }
    return 0;
}
