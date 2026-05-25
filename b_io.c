/**************************************************************
* Class::  CSC-415-0# Fall 2025
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
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
#include "fsLow.h"
#include "fsm.h"

extern VCB * vcb;
extern DirEntry * cwd;
extern int table[]; // FAT table pointer

#define MAXFCBS 20

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
	int currentBlock; 
	int startBlock; 
	int fileSize;
	int fileOffset;
	int dirEntryIndex;
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
b_io_fd b_open(char * filename, int flags)
	{
	b_io_fd returnFd;

	//*** TODO ***:  Modify to save or set any information needed
	//
	//

	if (startup == 0) b_init();

	// find file in directory
	int foundSpot = -1;
	int maxFiles = cwd[0].size / sizeof(DirEntry);

	for (int k =0; k < maxFiles; k++) {
		if (cwd[k].name[0] != '\0' && strcmp(cwd[k].name, filename) == 0) {
			foundSpot = k;
			break;
		}
	}

    // if not found then create a new file
	if (foundSpot == -1) {
		if (flags & O_CREAT) {
			// find free slot in directory
			int emptySlot = -1;
			int k = 0;
			for (k = 0; k < maxFiles; k++) {
				if (cwd[k].name[0] == '\0') {
					emptySlot = k;
					break;
				}
			}			
			if (emptySlot == -1) return -1; // directory is full

            // new file entry
			strcpy(cwd[emptySlot].name, filename);
			cwd[emptySlot].nameLength = strlen(filename);
			cwd[emptySlot].size = 0;
			cwd[emptySlot].isUsed = 1;
			cwd[emptySlot].sizeInBlocks = 0;
			cwd[emptySlot].startBlock = -1; // no data
			cwd[emptySlot].isDir = 0;
			cwd[emptySlot].flags = flags;

			foundSpot = emptySlot;
            
			// write update to disk
			int loc = vcb->dataStart + cwd[0].startBlock;
			LBAwrite(cwd, cwd[0].sizeInBlocks, loc);

		} else {
			return -1; // file not found
		}
	} else {
		// update flags for existing file
		cwd[foundSpot].flags = flags;

        // handle O_TRUNC by clearing file content if it exists
        if (flags & O_TRUNC) {
            // release existing blocks if there are any
            if (cwd[foundSpot].startBlock != -1) {
                freeChain(cwd[foundSpot].startBlock, vcb);
            }
            // reset file metadata
            cwd[foundSpot].size = 0;
            cwd[foundSpot].sizeInBlocks = 0;
            cwd[foundSpot].startBlock = -1;
        }
	}

	returnFd = b_getFCB();
	if (returnFd == -1) return -1;

    // initialize FCB
	b_fcb * fcb = &fcbArray[returnFd]; 

	fcb->buf = malloc(FS_BLOCKSIZE);
	if (fcb->buf == NULL) return -1;
	fcb->buflen = 0;
	fcb->index = 0;

    // copy info from directory 
	fcb->startBlock = cwd[foundSpot].startBlock;
    fcb->currentBlock = cwd[foundSpot].startBlock;
    fcb->fileSize = cwd[foundSpot].size;
    fcb->dirEntryIndex = foundSpot;
    
    // handle O_APPEND logic
    if (flags & O_APPEND) {
        fcb->fileOffset = fcb->fileSize;
        
        if (fcb->startBlock != -1) {
            int blockCount = fcb->fileOffset / FS_BLOCKSIZE;
            int curr = fcb->startBlock;
            for(int i = 0; i < blockCount; i++) {
                if(table[curr] != EOF) curr = table[curr];
            }
            fcb->currentBlock = curr;
            fcb->index = fcb->fileOffset % FS_BLOCKSIZE;
            
            // load block content so we can append to it
            LBAread(fcb->buf, 1, vcb->dataStart + fcb->currentBlock);
			
			// since we are at end of file, buffer is valid up to index
			fcb->buflen = fcb->index;
        }
    } else {
        fcb->fileOffset = 0;
    }
	return (returnFd);
    }

// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence) {
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS) || (fcbArray[fd].buf == NULL)) {
		return (-1); 			
	}

	b_fcb *fcb = &fcbArray[fd];
	int targetPos = 0; 

	switch (whence) {
		case SEEK_SET:
			targetPos = offset;
			break;
		case SEEK_CUR:
			targetPos = fcb->fileOffset + offset;
			break;
		case SEEK_END:
			targetPos = fcb->fileSize + offset;
			break;
		default:
			return -1; // invalid whence
	}
	
	// prevent seeking before start or past end of file
	if (targetPos < 0) targetPos = 0;
	if (targetPos > fcb->fileSize) targetPos = fcb->fileSize;

	fcb->fileOffset = targetPos;
    int blockOffset = targetPos / FS_BLOCKSIZE; 
	int byteOffset = targetPos % FS_BLOCKSIZE; 

	// update buffer
	fcb->index = byteOffset;
	fcb->buflen = 0;

	// find the right block walking FAT chain
	int curr = fcb->startBlock;
	if (blockOffset > 0) {
		for (int i = 0; i < blockOffset; i++) {
			// if chain breaks unexpectedly, stop
			if (curr == -1) break;
			curr = table[curr];
		}
	}

	fcb->currentBlock = curr;

	// load the block data immediately
	if (curr != -1) {
		// read the block from disk using VCB data offset
		LBAread(fcb->buf, 1, vcb->dataStart + curr);
		if (table[curr] == -1) { // end of FAT chain
				fcb->buflen = fcb->fileSize % FS_BLOCKSIZE;
				// ff file size is a perfect multiple of blocksize, the last block is full
				if (fcb->buflen == 0 && fcb->fileSize > 0) fcb->buflen = FS_BLOCKSIZE;
		} else {
				fcb->buflen = FS_BLOCKSIZE;
		}
	}

	return targetPos;
}


// Interface to write function	
int b_write (b_io_fd fd, char * buffer, int count) {
	
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS) || (fcbArray[fd].buf == NULL)) {
		return (-1); 					
	}

    b_fcb * fcb = &fcbArray[fd];

    // check if file is open
    if (!(cwd[fcb->dirEntryIndex].flags & (O_WRONLY | O_RDWR))) {
        return -1;
    }

	int bytesWritten = 0;
	int partToCopy;

	while (bytesWritten < count) {
		
        // check if its the first write to a new file
        if (fcb->currentBlock == -1) {
			fcb->currentBlock = allocateChain(1);
			fcb->startBlock = fcb->currentBlock;
			cwd[fcb->dirEntryIndex].startBlock = fcb->startBlock;
        }

        if (fcb->index == FS_BLOCKSIZE) {
            // write current full block
			LBAwrite(fcb->buf, 1, vcb->dataStart + fcb->currentBlock);

			// check FAT to see if we need a new block
			int nextBlock = table[fcb->currentBlock];

			if (nextBlock == EOF || nextBlock == 0) {
				nextBlock = allocateChain(1);
				if (nextBlock == -1) return -1; // disk is full

                // update FAT chain
				table[fcb->currentBlock] = nextBlock;
				table[nextBlock] = EOF;
                
				LBAwrite(table, vcb->FATBlocks, vcb->FATStart);
			}

			fcb->currentBlock = nextBlock;
			fcb->index = 0;
        }
        
        // calculate buffer space
		int spaceAvailable = FS_BLOCKSIZE - fcb->index;
		int remaining = count - bytesWritten;

		if (remaining < spaceAvailable) {
			partToCopy = remaining;
		} else {
			partToCopy = spaceAvailable;
		}

        // copy data to fcb buffer
		memcpy (fcb->buf + fcb->index, buffer + bytesWritten, partToCopy);
        
		fcb->index += partToCopy;
		bytesWritten += partToCopy;
		fcb->fileOffset += partToCopy;

        // if written past the end then update file size
		if (fcb->fileOffset > fcb->fileSize) {
			fcb->fileSize = fcb->fileOffset;
		}
	}	
		
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
int b_read (b_io_fd fd, char * buffer, int count) {

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS)) {
		return (-1); 					//invalid file descriptor
	}

	b_fcb *fcb = &fcbArray[fd];
	int bytesRead = 0;

	while (bytesRead < count && fcb->fileOffset < fcb->fileSize) {
		// load block if needed
		if (fcb->index == fcb->buflen) {
			if (fcb->currentBlock == -1) break;

			LBAread(fcb->buf, 1, vcb->dataStart + fcb->currentBlock);
			fcb->index = 0;

			// calculate bytes in block
		    int bytesInFile = fcb->fileSize - fcb->fileOffset;
			if (bytesInFile >= FS_BLOCKSIZE) {
                fcb->buflen = FS_BLOCKSIZE; 
            } else {
              fcb->buflen = bytesInFile;
            }
		}

        // copy data
		int available = fcb->buflen - fcb->index;
		int needed = count - bytesRead;
		int partsToCopy;

		if (needed < available) partsToCopy = needed;
		else partsToCopy = available;

		memcpy ( buffer + bytesRead, fcb->buf + fcb->index, partsToCopy);

		fcb->index += partsToCopy;
		bytesRead += partsToCopy;
		fcb->fileOffset += partsToCopy;

        // if done then advance block
		if (fcb->index == FS_BLOCKSIZE) {
			fcb->currentBlock = table[fcb->currentBlock];
             fcb->buflen = 0; 
             fcb->index = 0;
		}
	}

	return bytesRead;
}

// Interface to Close the file	
int b_close (b_io_fd fd) {
	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS)) {
		return (-1); 					//invalid file descriptor
	}

	b_fcb * fcb = &fcbArray[fd];

    if (fcb->buf != NULL && fcb->index > 0) {
        LBAwrite(fcb->buf, 1, vcb->dataStart + fcb->currentBlock);
    }

    // update dirEntry size
    int idx = fcb->dirEntryIndex;
    cwd[idx].size = fcb->fileSize;
    
    if (cwd[idx].startBlock == -1) {
        cwd[idx].startBlock = fcb->startBlock;
    }
    
    int total_blocks = 0;
    if (fcb->startBlock != -1) {
        int current = fcb->startBlock;
        while (current != EOF) {
            total_blocks++;
            current = table[current]; // move to next block in chain
        }
    }
cwd[idx].sizeInBlocks = total_blocks;

    int dirLBA = vcb->dataStart + cwd[0].startBlock;
    LBAwrite(cwd, cwd[0].sizeInBlocks, dirLBA);

	// free memory
	if(fcb->buf != NULL) {
		free(fcb->buf);
		fcb->buf = NULL;
	}

    fcb->currentBlock = 0;
    fcb->index = 0;
    fcb->buflen = 0;

	return 0;
}
