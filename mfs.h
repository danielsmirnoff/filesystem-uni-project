/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
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
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "b_io.h"

#include <dirent.h>
#define FT_REGFILE	DT_REG
#define FT_DIRECTORY DT_DIR
#define FT_LINK	DT_LNK

// Magic Number
#define VCB_MAGIC_NUMBER 0x415F5

#define MAX_ENTRIES 22

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif

extern int FS_BLOCKSIZE;

// Volume control block
typedef struct VCB {
	int magicNumber;
	int blockSize; // Size of a block
	int totalBlocks; // Total blocks in volume

	int FATStart; // Start block in FAT
	int FATBlocks; // Size of FAT in blocks

	int dataStart; // First block where data starts

	int rootStart; // First block in root
	int rootSize; // Size in bytes

	int freeBlocks; // Free blocks
} VCB; 

// Directory Entry 
typedef struct DirEntry {
	int flags; // Read only, write only, ect.
	int type; //Type of file (Dir or File)

	int startBlock; //First Block
	int size; // File size in bytes
	int sizeInBlocks;

	char name[40]; //File name
	int nameLength; //Size of name in bytes

	int isDir; //Directory flag, 0 if dir
	char isUsed; //Flag if entry is empty or not
	
} DirEntry;

extern DirEntry * Root;

// Parse Path Info
typedef struct PPinfo{
	DirEntry * parent;
	char * lastElementName;
	int index; // -1 if not exist
}PPinfo;
//Parse Path return values
// 0 : valid path
// -1: invalid path
int parsePath(char *pathName, PPinfo *ppi);
//Find in Directory return values
// <= 0: The index in Directory array
// -1  : Not found
int findInDirectory(DirEntry *parent, char *token);
DirEntry *findDirInDirectory(DirEntry *parent, char *token);
//Free Space Functions
int createBitmap(int bitmapBlocks);
int findFreeBlocks(int count);

//Directory Functions
void writeDirToDisk(DirEntry *newDir);
DirEntry *createDir(int minEntries, DirEntry* parent);
DirEntry *loadDir(DirEntry *directory);

// This structure is returned by fs_readdir to provide the caller with information
// about each file as it iterates through a directory
struct fs_diriteminfo
	{
    unsigned short d_reclen;    /* length of this record */
    unsigned char fileType;    
    char d_name[255]; 			/* filename max filename is 255 characters */
	};

// This is a private structure used only by fs_opendir, fs_readdir, and fs_closedir
// Think of this like a file descriptor but for a directory - one can only read
// from a directory.  This structure helps you (the file system) keep track of
// which directory entry you are currently processing so that everytime the caller
// calls the function readdir, you give the next entry in the directory
typedef struct
	{
	/*****TO DO:  Fill in this structure with what your open/read directory needs  *****/
	unsigned short  d_reclen;		/* length of this record */
	unsigned short	dirEntryPosition;	/* which directory entry position, like file pos */
	int totalEntries;                   /*Total number of entries in this file*/
	DirEntry*	    directory;			/* Pointer to the loaded directory you want to iterate */
	struct fs_diriteminfo * di;		/* Pointer to the structure you return from read */
	} fdDir;

// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);

// Directory iteration functions
fdDir * fs_opendir(const char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// Misc directory functions
char * fs_getcwd(char *pathname, size_t size);
int fs_setcwd(char *pathname);   //linux chdir
int fs_isFile(char * filename);	//return 1 if file, 0 otherwise
int fs_isDir(char * pathname);		//return 1 if directory, 0 otherwise
int fs_delete(char* filename);	//removes a file


// This is the strucutre that is filled in from a call to fs_stat
struct fs_stat
	{
	off_t     st_size;    		/* total size, in bytes */
	blksize_t st_blksize; 		/* blocksize for file system I/O */
	blkcnt_t  st_blocks;  		/* number of 512B blocks allocated */
	time_t    st_accesstime;   	/* time of last access */
	time_t    st_modtime;   	/* time of last modification */
	time_t    st_createtime;   	/* time of last status change */
	
	/* add additional attributes here for your file system */
	};

int fs_stat(const char *path, struct fs_stat *buf);

#endif

