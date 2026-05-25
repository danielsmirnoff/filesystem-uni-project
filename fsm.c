/**************************************************************
* Class::  CSC-415-01 Fall 2025
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: fsm.c
*
* Description:: The free space management driver
*
* This is where the implementation of the free space management
* system lives.
*
**************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "fsm.h"
#include "mfs.h"
#include "fsLow.h"

int table[MaxEntries];

//Allocates a portion of the disk to be used by the freespace map
//Returns the index where the freespace map starts
int initFSM(VCB *vcb) {

    //Error checking
    if(vcb == NULL) {
        printf("\nFreeSpace No VCB\n");
        return -1;
    }
    if(vcb->blockSize <= 0 || vcb->totalBlocks <= 0) {
        printf("\nInit FreeSpace Error\n");
        return -1;
    }

    //Checks if it needs to format within the function
    //Formats the volume
    formatFSM(1, vcb);
    
    return 1;
}

//Clears the freespace map from memory
//Call when fs is shutdown
void freeFSM() {
}

//Formats the volume by setting all the values FREE
void formatFSM(int startingBlock, VCB *vcb) {
    //Checks if needs to format
    //To force add a flag
    if(vcb->magicNumber == VCB_MAGIC_NUMBER || UsePersistance == 1) return;

    //Calculate space
    int bytesNeeded = vcb->totalBlocks * sizeof(int); //4 bytes (1 int) for each block
    int blocksNeeded = (bytesNeeded + (vcb->blockSize - 1)) / vcb->blockSize;
    int actualBytes = blocksNeeded * vcb->blockSize;

    //Allocate buffer
    char *buffer = malloc(vcb->blockSize);
    if(buffer == NULL) {
        printf("\nFree Space Buffer Error\n");
        exit(-1);
    }

    //Format volume
    //Fill buffer with 0
    int blocksTraversed = 0;
    int bytesTraversed = 0;
    while(bytesTraversed < vcb->blockSize) {
        buffer[bytesTraversed] = FREE;
        bytesTraversed++;
    }
    //Write to disk
    //Iterate through all blocks and clear block on disk
    for (int i = 0; i < blocksNeeded; i++)
    {
        LBAwrite(buffer, 1, i + startingBlock);
    }
    
    //Debug/Testing //TODO remove when done
    printf("\nBlocks allocated for freespace map: %d \n", blocksNeeded);

    //Test Allocate
    printf("\nTesting a write of length %d in FSM\n", 5);
    int head = allocateChain(5);

    //Cleanup
    free(buffer);

}

//Allocates a chain in the FAT given a length
//Sets the the chain
//Returns the index of the head of the chain for FAT or -1 if failed
int allocateChain(int length) {
    if(D == 1) printf("\nAllocating a chain of length: %d", length); //TODO remove

    //This part finds a free area in the FAT to allocate a chain of length
    //Iterate through FAT to find a free space, if free set table index and prev index
    //if not free keep walking
    int head = -2;
    int prev = -1;
    int currentLength = 0;
    int i = 0; //Checks for freespaces starting at block 1 //TODO account for vcb pos
    //Find chain
    for(;;) {
        //If entry is empty allocate its space to the chain
        if(table[i] == FREE) {
            if(D == 1) printf("\nCurrent: %d | prev: %d", i, prev); //TODO remove
            //Set head of chain
            if(head == -2) { //Checks if chain node is first
                head = i;
                prev = i;
                if(D == 1) printf(" (HEAD)"); //TODO remove
            }
            //Set prev node of chain to current
            if(prev != EOF) {
                table[prev] = i;
            }
            //If end of chain, set EOF and return
            if(currentLength == length) {
                table[i] = EOF;
                break;
            }
            prev = i;
            currentLength++;
        }
        i++;
        
    }

    //TODO optimize this later
    //Calculate space
    //Size of free space chain on disk
    int startBlock = getBlockAtIndex(head); //Where the head starts in terms of blocks
    //Index of the first entry in the FAT of the block
    int startBlockFATIndex = 0; //TODO calculate first index of block

    int bytesNeeded = currentLength * sizeof(int); 
    int blocksNeeded = (bytesNeeded + (FS_BLOCKSIZE - 1)) / FS_BLOCKSIZE;
    int actualBytes = blocksNeeded * FS_BLOCKSIZE;
    int index = head;
    if(D == 1) printf("\nWriting chain to disk\n"); //TODO remove
    
    //Allocate buffer
    char* buffer = malloc(actualBytes);
    if(buffer == NULL) {
        printf("Allocate malloc error\n");
        return -1;
    }

    //Walk through FAT in order starting from head and add it to buffer
    int bytesWritten = 0;
    int j = startBlockFATIndex;
    while(bytesWritten < actualBytes) {
        //printf("Buffering: %d\n", buffer[j]); //Debug
        buffer[j] = table[j];
        index++;
        j++;
        bytesWritten+=sizeof(int);
    }
    if (D == 1) printf("Bytes Written: %d\n", bytesWritten);//TODO remove
    if(D == 1) printf("Writing Chain at block: %d\n", startBlock); //TODO remove

    //Write to disk
    LBAwrite(buffer, blocksNeeded, startBlock);

    if(D == 1) printf("Chain ends at block: %d\n", startBlock + blocksNeeded); //TODO remove

    free(buffer);
    return head;
}


//Frees a chain in the FAT table
void freeChain(int head, VCB *vcb) {
    if (head < 0 || head >= MaxEntries) {
        return;
    }

    int current = head; 
    int next; 
    int freedCount = 0;

    // keep going until we hit the end of the file
    while (current != -1 && table[current] != -1) {
        next = table[current]; // save next spot before wiping current one
        table[current] = FREE; // wipe current block
        freedCount++;
        current = next;
    }

    // have to free the last block
    if (current != -1 && current < MaxEntries) {
        table[current] = FREE;
        freedCount++;
    }

    
    // updating vcb 
    if (vcb != NULL) {
        vcb->freeBlocks += freedCount;

        LBAwrite(vcb, 1, 1); // write back to disk
    }

    LBAwrite(table, vcb->FATBlocks, vcb->FATStart); // save table to disk
}

//Get the block that contains the index
int getBlockAtIndex(int index) {
    int maxIndices = FS_BLOCKSIZE / sizeof(index);
    int blockIndex = (index + (maxIndices-1))/maxIndices;
    return blockIndex == 0 ? 1 : blockIndex;
}



