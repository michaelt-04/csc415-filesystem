/**************************************************************
* Class::  CSC-415-01 Spring 2025
* Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
* Student IDs:: 922525848, 922707016, 922711514, 918371654
* GitHub-Name:: Jasuv
* Group-Name:: Debug Thugs
* Project:: Basic File System
*
* File:: freeSpace.h
*
* Description:: 
*	Header structure skeleton for initializing and allocating free space
*
**************************************************************/


#ifndef FREESPACE_H
#define FREESPACE_H

extern char* freeSpaceMap;
extern int freeSpaceMapSize;

int initFreeSpace(int blockCount, int sizeOfBlock); // Initialize Free Space on disk.
int* allocateBlocks(int count); // Allocates blocks 'count' times, returns an array of allocated blocks.
int freeBlocks(int* blockArray, int count); // Set block free for given block index. 
int checkBlockAvailability(int blockIndex); // Check block availability return 0 for used, 1 for free.
int loadFreeSpaceMap(int blockSize, int startBlock, int totalBlockCount); // Load the freespacemap when reinitializing file system. 


#endif
