/**************************************************************
* Class::  CSC-415-02 Spring 2025
* Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
* Student IDs:: 922525848, 922707016, 922711514, 918371654
* GitHub-Name:: Jasuv
* Group-Name:: Debug Thugs
* Project:: Basic File System
*
* File:: b_io.c
*
* Description:: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "mfs.h"
#include <fsLow.h>
#include <freeSpace.h>

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer

	// Added information
	de_struct* fi;
	de_struct* parent_dir;
	int current_block;
	int start_block;
	int flags;
	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open(char * filename, int flags) {
    if (startup == 0) b_init();  //Initialize our system
    
    b_io_fd returnFd;
    
    char path_copy[LOCAL_PATH_MAX];
    strncpy(path_copy, filename, LOCAL_PATH_MAX - 1);
    path_copy[LOCAL_PATH_MAX - 1] = '\0';
    
    returnFd = b_getFCB();
    if (returnFd < 0) {
        printf("No free FCB slots available\n");
        return -1;
    }
    
    fcbArray[returnFd].buf = malloc(B_CHUNK_SIZE);
    if (fcbArray[returnFd].buf == NULL) {
        printf("Memory allocation failed for file buffer\n");
		fcbArray[returnFd].buf = NULL;
		fcbArray[returnFd].fi = NULL;
		fcbArray[returnFd].parent_dir = NULL;
        return -1;
    }
    
    parseInfo *ppi = malloc(sizeof(parseInfo));
    if (ppi == NULL) {
        printf("Memory allocation failed for parseInfo\n");
        free(fcbArray[returnFd].buf);
		fcbArray[returnFd].buf = NULL;
		fcbArray[returnFd].fi = NULL;
		fcbArray[returnFd].parent_dir = NULL;
        return -1;
    }
    
    int result = parsePath(path_copy, ppi);
    
    // if file doesnt exist, create it
    if (result != 0 || ppi->index < 0) {

        // if O_CREAT flag is set, create the file
        if (flags & O_CREAT) {
            de_struct *parentDir = ppi->parent;
            
            // Find an empty slot in the parent directory
            int emptySlot = -1;
            for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
                if (parentDir[i].file_name[0] == '\0') {
                    emptySlot = i;
                    break;
                }
            }
            
            if (emptySlot == -1) {
                printf("Parent directory is full, cannot create file\n");
                free(ppi);
                free(fcbArray[returnFd].buf);
				fcbArray[returnFd].buf = NULL;
				fcbArray[returnFd].fi = NULL;
				fcbArray[returnFd].parent_dir = NULL;
                return -1;
            }
            
            // Allocate a block for the new file
            int *newFileBlocks = allocateBlocks(1);
            if (newFileBlocks == NULL) {
                printf("Failed to allocate block for new file\n");
                free(ppi);
                free(fcbArray[returnFd].buf);
				fcbArray[returnFd].buf = NULL;
				fcbArray[returnFd].fi = NULL;
				fcbArray[returnFd].parent_dir = NULL;
                return -1;
            }
            
            // get file name from path
            char *fileName = ppi->lastElementName;
            if (fileName == NULL) {
                // Get filename from the original path if necessary
                fileName = strrchr(filename, '/');
                if (fileName == NULL) {
                    fileName = filename;
                } else {
                    fileName++;
                }
            }

			//printf("updating parentDir[%d] to %s\n", emptySlot, fileName);
            
            // init the new file entry
            time_t now = getTime();
            strcpy(parentDir[emptySlot].file_name, fileName);
            parentDir[emptySlot].size = 0;
            parentDir[emptySlot].mode = 0777; 
            parentDir[emptySlot].blocks_allocated[0] = newFileBlocks[0];
            parentDir[emptySlot].blocks_count = 1;
            parentDir[emptySlot].date_created = now;
            parentDir[emptySlot].date_modified = now;
            parentDir[emptySlot].is_directory = 0;
            
            // write the updated parent directory to disk
		   for (int i = 0; i < parentDir[0].blocks_count; i++) {
				void * dirToBlocks = (void *)((char *)parentDir + i * BLOCK_SIZE);
				if (LBAwrite(dirToBlocks, 1, parentDir[0].blocks_allocated[i]) != 1) {
					printf("Error writing updated block %d for parent directory\n", i);
							if (parentDir != rootDir && parentDir != cwDir) {
								free(parentDir);
							}
							fcbArray[returnFd].buf = NULL;
							fcbArray[returnFd].fi = NULL;
							fcbArray[returnFd].parent_dir = NULL;
					return -1;
				}
			}
            
            // Update FCB with the new file information
            fcbArray[returnFd].fi = &parentDir[emptySlot];
            fcbArray[returnFd].flags = flags;
            fcbArray[returnFd].index = 0;
            fcbArray[returnFd].current_block = 0;
			fcbArray[returnFd].start_block = newFileBlocks[0];
			fcbArray[returnFd].parent_dir = parentDir;
            
            free(newFileBlocks);
        } else {
            // File doesn't exist and O_CREAT not specified
            printf("File not found: %s\n", filename);
            free(ppi);
            free(fcbArray[returnFd].buf);
			fcbArray[returnFd].buf = NULL;
			fcbArray[returnFd].fi = NULL;
			fcbArray[returnFd].parent_dir = NULL;
            return -1;
        }
    } else {

        // File exists
        de_struct *entry = &ppi->parent[ppi->index];
        
        // Check if it's a directory
        if (entry->is_directory) {
            printf("Cannot open directory as file: %s\n", filename);
            free(ppi);
            free(fcbArray[returnFd].buf);
			fcbArray[returnFd].buf = NULL;
			fcbArray[returnFd].fi = NULL;
			fcbArray[returnFd].parent_dir = NULL;
            return -1;
        }
        
        // Handle O_TRUNC flag - reset file size to 0
        if (flags & O_TRUNC) {
            entry->size = 0;
            time_t now = getTime();
            entry->date_modified = now;
            
            // Write the updated entry back to disk
		    for (int i = 0; i < ppi->parent->blocks_count; i++) {
				void * dirToBlocks = (void *)((char *)ppi->parent + i * BLOCK_SIZE);
				if (LBAwrite(dirToBlocks, 1, ppi->parent->blocks_allocated[i]) != 1) {
					printf("Error writing updated block %d for parent directory\n", i);
							if (ppi->parent != rootDir && ppi->parent != cwDir) {
								free(ppi->parent);
							}
							fcbArray[returnFd].buf = NULL;
							fcbArray[returnFd].fi = NULL;
							fcbArray[returnFd].parent_dir = NULL;
					return -1;
				}
			}
        }
        
        // For read or read/write, load the first block
        if ((flags & O_RDONLY) || (flags & O_RDWR)) {
            // Only try to read if the file has content
            if (entry->size > 0) {
                int blockNum = entry->blocks_allocated[0];
                int blocksRead = LBAread(fcbArray[returnFd].buf, 1, blockNum);
                if (blocksRead <= 0) {
                    printf("Failed to read block %d for file %s\n", blockNum, filename);
                    free(fcbArray[returnFd].buf);
                    free(ppi);
					fcbArray[returnFd].buf = NULL;
					fcbArray[returnFd].fi = NULL;
					fcbArray[returnFd].parent_dir = NULL;
                    return -1;
                }
                fcbArray[returnFd].buflen = (entry->size < B_CHUNK_SIZE) ? entry->size : B_CHUNK_SIZE;
            } else {
                // Empty file
                fcbArray[returnFd].buflen = 0;
            }
        }
        
        // Set up FCB entry
        fcbArray[returnFd].fi = entry;
        fcbArray[returnFd].flags = flags;
        fcbArray[returnFd].index = 0;
        fcbArray[returnFd].current_block = 0;
		fcbArray[returnFd].start_block = entry->blocks_allocated[0];
		fcbArray[returnFd].parent_dir = ppi->parent;

    }

    free(ppi);
    return returnFd;		// all set
}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
		
	return (0); //Change this
	}


// Interface to write function    
int b_write(b_io_fd fd, char *buffer, int count) {
    if (startup == 0) b_init();  // Initialize our system

    // check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) 
		{
        return (-1);  					// invalid file descriptor
    	}

    // check if file is open
    if (fcbArray[fd].fi == NULL) {
        return -1;  // File not open
    }

    // check write permission
    if (!(fcbArray[fd].flags & O_WRONLY) && !(fcbArray[fd].flags & O_RDWR)) {
        return -1;  // File not opened for writing
    }

    int bytesWritten = 0;      // bytes written so far
    int bytesToWrite = count;  // bytes remaining to write
    int currentPos = 0;        // current position in buffer

    // if we have data in the buffer already
    if (fcbArray[fd].index > 0) {
        // how much space remains in the current buffer
        int remainingBufferSpace = B_CHUNK_SIZE - fcbArray[fd].index;
        
        // write to the buffer as much as will fit
        int bytesToCopy = (bytesToWrite < remainingBufferSpace) ? bytesToWrite : remainingBufferSpace;
        
        memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, bytesToCopy);
        fcbArray[fd].index += bytesToCopy;
        bytesWritten += bytesToCopy;
        currentPos += bytesToCopy;
        bytesToWrite -= bytesToCopy;
        
        // if buffer is full, write it to disk
        if (fcbArray[fd].index == B_CHUNK_SIZE) {
            // Write the block to disk
		    if (LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].start_block + fcbArray[fd].current_block) != 1) {
                return bytesWritten;  
            }
        
            // clear buffer for next block
            fcbArray[fd].index = 0;
            fcbArray[fd].current_block++;
        }
    }

    // write whole blocks directly from the buffer
    while (bytesToWrite >= B_CHUNK_SIZE) {
        // Write block directly from user's buffer
        if (LBAwrite(buffer + currentPos, 1, fcbArray[fd].start_block+fcbArray[fd].current_block) != 1) {
            return bytesWritten; 
        }
        
        bytesWritten += B_CHUNK_SIZE;
        currentPos += B_CHUNK_SIZE;
        bytesToWrite -= B_CHUNK_SIZE;
        fcbArray[fd].current_block++;
    }

    // copy any remaining bytes to buffer
    if (bytesToWrite > 0) {
        memcpy(fcbArray[fd].buf, buffer + currentPos, bytesToWrite);
        fcbArray[fd].index = bytesToWrite;
        bytesWritten += bytesToWrite;
    }

    // update file size
    int newSize = fcbArray[fd].fi->size;
    int bytesWrittenToFile = bytesWritten;
    
    // calculate how many bytes are written beyond the current file size
	int currentLoc = fcbArray[fd].current_block * B_CHUNK_SIZE + fcbArray[fd].index;

    if (currentLoc > newSize) {
        newSize = currentLoc;
        
        // update file size and modification time
        fcbArray[fd].fi->size = newSize;
        fcbArray[fd].fi->date_modified = getTime();
        
        // make sure we have enough blocks allocated
        int blocksNeeded = (newSize + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE;
        if (blocksNeeded > fcbArray[fd].fi->blocks_count) {
            // Allocate more blocks if needed
            int additionalBlocks = blocksNeeded - fcbArray[fd].fi->blocks_count;
            int *newBlocks = allocateBlocks(additionalBlocks);
            
            if (newBlocks == NULL) {
                // Could not allocate more blocks
                return bytesWritten;
            }
            
            for (int i = 0; i < additionalBlocks; i++) {
                if (fcbArray[fd].fi->blocks_count < MAX_DE_BLOCK_COUNT) {
                    fcbArray[fd].fi->blocks_allocated[fcbArray[fd].fi->blocks_count] = newBlocks[i];
                    fcbArray[fd].fi->blocks_count++;
                }
            }
            
            free(newBlocks);
        }
        
        // Write directory entry back to disk to update info
		de_struct *parentDir = fcbArray[fd].parent_dir;

        for (int i = 0; i < parentDir[0].blocks_count; i++) {
            void *dirToBlocks = (void *)((char *)parentDir + i * BLOCK_SIZE);
            if (LBAwrite(dirToBlocks, 1, parentDir[0].blocks_allocated[i]) != 1) {
                printf("Error writing updated block %d for parent directory\n", i);
                return bytesWritten;
            }
        }
		
    }

	//printf("bytesWritten: %d\n", bytesWritten);

    return bytesWritten;
}



// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{

    if (startup == 0) b_init();  //Initialize our system
    
	// check that fd is between 0 and (MAXFCBS-1)
    if (fd < 0 || fd >= MAXFCBS) {
        return (-1);  // invalid descriptor
    }
    
    if (fcbArray[fd].fi == NULL) {		// File is not open for this fd
        return -1;  // File not open
    }
    
    int bytesReturned = 0;			// what we will return
    int bytesRemaining= count;
    int bufferPos = 0;
    int availableInBuffer = fcbArray[fd].buflen - fcbArray[fd].index;	// holds how many bytes are left in my buffer
    
    // handle EOF
    int bytesRead = (fcbArray[fd].current_block * BLOCK_SIZE) - availableInBuffer;
    if ((bytesRead + count) > fcbArray[fd].fi->size) {
        bytesRemaining = fcbArray[fd].fi->size - bytesRead;
        if (bytesRemaining <= 0){
			return 0;  // Nothing left to read
		} 
    }
    
    // Part 1: copy data from existing buffer if available
    if (availableInBuffer > 0) {
        int amountTransferred;
		if(bytesRemaining < availableInBuffer){
			amountTransferred = bytesRemaining;
		}else{
			amountTransferred = availableInBuffer;
		}
        
        memcpy(buffer, fcbArray[fd].buf + fcbArray[fd].index, amountTransferred);
        fcbArray[fd].index += amountTransferred;
        bufferPos += amountTransferred;
        bytesRemaining -= amountTransferred;
        bytesReturned += amountTransferred;
    }
    
    // Part 2: read whole blocks directly
    if (bytesRemaining >= BLOCK_SIZE) {
        int blocksToRead = bytesRemaining / BLOCK_SIZE;
        int blockPos = fcbArray[fd].start_block + fcbArray[fd].current_block;
        
        int blocksRead = LBAread(buffer + bufferPos, blocksToRead, blockPos);
        
        int bytesRead = blocksRead * BLOCK_SIZE;
        fcbArray[fd].current_block += blocksRead;
        bufferPos += bytesRead;
        bytesRemaining -= bytesRead;
        bytesReturned += bytesRead;
    }
    
    // Part 3: read final partial block 
    if (bytesRemaining > 0) {
        int blockPos = fcbArray[fd].start_block + fcbArray[fd].current_block;
        int bytesRead = LBAread(fcbArray[fd].buf, 1, blockPos) * BLOCK_SIZE;
        
        if (bytesRead > 0) {

			int amountTransferred;
			if(bytesRemaining < bytesRead){
				amountTransferred = bytesRemaining;
			}else{
				amountTransferred = bytesRead;
			}
            
            memcpy(buffer + bufferPos, fcbArray[fd].buf, amountTransferred);
            fcbArray[fd].index = amountTransferred;
            fcbArray[fd].buflen = bytesRead;
            fcbArray[fd].current_block++;
            bytesReturned += amountTransferred;
        }
    }
    
    return bytesReturned;
	}
	
// Interface to Close the file	
int b_close (b_io_fd fd)
	{

		// check that fd is between 0 and (MAXFCBS-1)
		if ((fd < 0) || (fd >= MAXFCBS)) 
		{
        return (-1);  					// invalid file descriptor
    	}

		// check if file is open
		if (fcbArray[fd].fi == NULL) {
			return -1;  // File not open
		}

		// flush any remaining data from the buffer before closing file
		if (fcbArray[fd].index > 0) {
			// only write if the file was opened with write permissions
			if ((fcbArray[fd].flags & O_WRONLY) || (fcbArray[fd].flags & O_RDWR)) {
				// write to disk
				if (LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].start_block + fcbArray[fd].current_block) != 1) {
					printf("Error writing final buffer in b_close\n");
				}
			}
			fcbArray[fd].buflen = 0;
		}

		free(fcbArray[fd].buf);
		fcbArray[fd].buf = NULL;
		fcbArray[fd].fi = NULL;
		fcbArray[fd].parent_dir = NULL;

		return 0;
	}
