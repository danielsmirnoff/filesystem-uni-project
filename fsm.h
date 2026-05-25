/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: fMng.h
*
* Description:: 
*	This is the file system interface.
*	This is the interface needed by the driver to interact with
*	your filesystem.
*
**************************************************************/

#include "mfs.h"

#ifndef _FSM_H
#define _FSM_H

#define UsePersistance 0
#define MaxEntries 4096
#define D 0 //Debugging //0 = off | 1 = on

#define FREE 0 //A characters that the FSM formats with

//Global FAT table
extern int table[MaxEntries]; //TODO fix this switch to extern


#pragma FSMCore
//Allocates a portion of the disk to be used by the freespace map
//Returns the index where the freespace map starts
int initFSM(VCB *vcb);

//Clears the freespace map from memory
//Call when fs is shutdown
void freeFSM();

//Formats the volume by setting all the values to 0x0
void formatFSM(int startingBlock, VCB *vcb);

#pragma endregion


#pragma FSMBlockManagement

//Allocates a chain in the FAT to be used by a file
//Returns the head of the chain
int allocateChain(int length);

//Frees a chain in the FAT table
void freeChain(int head, VCB* vcb);

//Get the block that contains the index
int getBlockAtIndex(int index);

#pragma endregion

#endif