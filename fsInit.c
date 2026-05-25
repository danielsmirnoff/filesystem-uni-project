/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: fsInit.c
*
* Description:: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "fsm.h"
#include "mfs.h"

int FS_BLOCKSIZE;

DirEntry * Root;
VCB * vcb;
DirEntry * cwd;

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
	{

	/* TODO: Add any code you need to initialize your file system. */
	FS_BLOCKSIZE = blockSize;
	
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
	vcb = malloc(sizeof(VCB));
	

	// vcb struct
	vcb->blockSize = blockSize;
    vcb->totalBlocks = numberOfBlocks;
    vcb->magicNumber = VCB_MAGIC_NUMBER;


	//Initialize FAT/free space
	vcb->FATStart = 2; 
    vcb->FATBlocks = ((numberOfBlocks * sizeof(int)) + blockSize - 1) / blockSize;
    vcb->dataStart = vcb->FATStart + vcb->FATBlocks;

	initFSM(vcb);

	//Initialize root
	Root = createDir(22, NULL);
	if (Root == NULL) {
        printf("Error: Failed to create root directory.\n");
        return -1;
    }

	//Provide VCB with free space and root information
	vcb->rootStart = Root->startBlock;
	vcb->rootSize = Root->size;

	LBAwrite(vcb, 1, 1); // save everything to disk

	//Set global CWD variable
	cwd = Root;

	LBAwrite(vcb, 1, 1); // save everything to disk

	return 0;
	}
	
	
void exitFileSystem () {
	printf ("System exiting\n");
	freeFSM(); 
	}