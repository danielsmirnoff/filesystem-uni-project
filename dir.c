#define _GNU_SOURCE
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mfs.h"
#include "fsm.h"
#include "fsLow.h"

extern VCB * vcb;
extern DirEntry * cwd; //pointer to working directory in memory
extern DirEntry * Root;   // root directory (already used in parsePath)

// Track current working directory as a string like "/", "/dir", "/dir/subdir"
#define MAX_PATH_LENGTH 4096
static char currentPath[MAX_PATH_LENGTH] = "/";

// Helper prototypes for path handling
static int normalizePath(const char *input, char *output, size_t outSize);
static DirEntry *resolveAbsolutePath(const char *absPath);

//Helpers
static int getNextBlock(int currentBlock) {
    if(currentBlock < 0 || currentBlock >= MaxEntries) {
        return -1;
    }
    int next = table[currentBlock];

    if(next <= 0) {
        return -1;
    }

    return next;
}

void freePPInfo(PPinfo *ppi) {
    if(ppi->parent != Root && ppi->parent != cwd) {
        free(ppi->parent);
    }
    free(ppi->lastElementName);
}
// static DirEntry* parsePath(const char* path) {
//     //DirEntry* dirEntry = malloc(sizeof(DirEntry));
//     // if(dirEntry == NULL) {
//     //     return NULL;
//     // }

//     return NULL;
// }


char * fs_getcwd(char *pathname, size_t size) {
    if (pathname == NULL || size == 0) {
        return NULL;
    }
    strcpy(pathname, currentPath);

    if (currentPath[0] == '\0') {
        currentPath[0] = '/';
        currentPath[1] = '\0';
    }

    size_t len = strlen(currentPath);
    if (len + 1 > size) {
        return NULL;
    }

    memcpy(pathname, currentPath, len + 1);
    return pathname;
}
int fs_setcwd(char *pathname) {
    if (pathname == NULL || pathname[0] == '\0') {
        return -1;
    }

    char combined[MAX_PATH_LENGTH];
    char normalized[MAX_PATH_LENGTH];

    if (pathname[0] == '/') {
        if (strlen(pathname) >= sizeof(combined)) {
            return -1;
        }
        strcpy(combined, pathname);
    } else {
        if (strcmp(currentPath, "/") == 0) {
            if (snprintf(combined, sizeof(combined), "/%s", pathname) >= (int)sizeof(combined))
                return -1;
        } else {
            if (snprintf(combined, sizeof(combined), "%s/%s", currentPath, pathname) >= (int)sizeof(combined))
                return -1;
        }
    }

    if (normalizePath(combined, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    DirEntry *newDir;
    if (strcmp(normalized, "/") == 0) {
        newDir = Root;
    } else {
        newDir = resolveAbsolutePath(normalized);
        if (newDir == NULL) {
            return -1;
        }
    }

    if (cwd != NULL && cwd != Root && cwd != newDir) {
        free(cwd);
    }
    cwd = newDir;

    strncpy(currentPath, normalized, sizeof(currentPath) - 1);
    currentPath[sizeof(currentPath) - 1] = '\0';

    return 0;
}
int fs_isFile(char * filename) {
    PPinfo ppInfo;
    int ret = parsePath(filename, &ppInfo);
    
    if (ret < 0) return 0;

    if (ppInfo.index == -2) return 0;
    if (ppInfo.parent[ppInfo.index].isDir == 0) {
        return 1; // is a file
    }
    
    return 0; // not a file
}

int fs_isDir(char * pathname) {
    PPinfo ppInfo;
    int ret = parsePath(pathname, &ppInfo);
    
    if (ret < 0) return 0;

    if (ppInfo.index == -2) return 1;
    if (ppInfo.parent[ppInfo.index].isDir == 1) {
        return 1; // its a directory
    }
    
    return 0; // not a dir
}
int fs_mkdir(const char *pathname, mode_t mode) {
    //printf("\nMaking Dir at: %s", pathname);
    if(pathname == NULL || pathname[0] == '\0') {
        return -1;
    }

    //Create a copy of path
    char *pathCopy = strdup(pathname);
    if(pathCopy == NULL) {
        return -1;
    }

    //Parse Path
    PPinfo ppi;
    int ppReturn = parsePath(pathCopy, &ppi);
    if(ppReturn == -1) {
        free(pathCopy);
        freePPInfo(&ppi);
        return -1;
    }

    //Check Parent dir
    if(ppi.parent == NULL) {
        ppi.parent = Root;
    }

    //Check if dir exists
    if(ppi.index != -1) {
        free(pathCopy);
        freePPInfo(&ppi);
        return -1;
    }

    //TODO add directory name checking

    //Find first free entry
    int entryCount = ppi.parent[0].size / sizeof(DirEntry);
    int freeEntryIndex = -1;
    for (int i = 2; i < entryCount; i++)
    {
        if(ppi.parent[i].isUsed == 0) {
            freeEntryIndex = i;
            break;
        }
    }
    //If no free space left
    if(freeEntryIndex == -1) {
        free(pathCopy);   
        freePPInfo(&ppi);
        return -1;
    }
    
    //Create Directory
    DirEntry *newDir = createDir(22, ppi.parent); //TODO Use a max entries variable instead
    DirEntry *freeDir = &ppi.parent[freeEntryIndex];

    if(newDir == NULL) {
        free(pathCopy);
        freePPInfo(&ppi);
        return -1;
    }

    //Copy name
    strcpy(freeDir->name, ppi.lastElementName);
    freeDir->name[sizeof(ppi.parent[freeEntryIndex].name) - 1] = '\0';
    freeDir->nameLength = strlen(freeDir->name);

    freeDir->size = newDir[0].size;
    freeDir->sizeInBlocks = newDir[0].sizeInBlocks;
    freeDir->startBlock = newDir[0].startBlock;
    freeDir->isDir = 1;
    freeDir->isUsed = 1;
    freeDir->type = 1;
    freeDir->flags = 0;

    //Write dir
    int parentLBA = vcb->dataStart + ppi.parent[0].startBlock;
    int writeResult = LBAwrite(ppi.parent, ppi.parent[0].sizeInBlocks, parentLBA);

    //Write Failed
    if(writeResult != ppi.parent[0].sizeInBlocks) {
        free(pathCopy);
        freeChain(newDir[0].startBlock, vcb);
        free(newDir);
        freePPInfo(&ppi);
    }

    free(newDir);
    free(pathCopy);
    freePPInfo(&ppi);

    return 0;
}

DirEntry *createDir(int minEntries, DirEntry* parent){
    //printf("\nCreating Dir");

    //Calculate Space needed
    int bytesNeeded = minEntries * sizeof(DirEntry);
    int blocksNeeded = (bytesNeeded + (FS_BLOCKSIZE-1))/FS_BLOCKSIZE;
    int bytesToMalloc = blocksNeeded * FS_BLOCKSIZE;
    int actualEntriesCount = bytesToMalloc / sizeof(DirEntry);
    DirEntry * new = malloc(bytesToMalloc);
    if(new == NULL) return NULL;

    //Clear memory
    memset(new, 0, bytesToMalloc);

    //Initialize entries
    for(int i = 2; i < actualEntriesCount; i++){
        new[i].isUsed = 0;
    }

    //Set Current
    strcpy(new[0].name, ".");
    new[0].nameLength = strlen(new[0].name);
    new[0].size = actualEntriesCount * sizeof(DirEntry);
    int sizeInBlocks = (new[0].size + (FS_BLOCKSIZE-1))/FS_BLOCKSIZE;
    new[0].sizeInBlocks = sizeInBlocks;
    new[0].isDir = 1;
    new[0].startBlock = allocateChain(blocksNeeded);
    new[0].isUsed = 1;
    
    //Check if root directory
    if(parent == NULL){
        parent = new;
    }

    //Set Parent
    strcpy(new[1].name, "..");
    new[1].nameLength = strlen(new[1].name);
    new[1].size = parent->size;
    new[1].sizeInBlocks = parent->sizeInBlocks;
    new[1].startBlock = parent->startBlock;
    new[1].isDir = parent->isDir;
    new[1].isUsed = 1;

    //Write new directory to disk
    LBAwrite(new, sizeInBlocks, new[0].startBlock + 154);
    //printf("\nCreating dir at block: %d\n", new[0].startBlock+154);
    //void*buffer = calloc(512,1); // 0 buffer to clear if needed (for testing)
    //printf("Root Block Start: %d With name %s and size %d\n", new[0].startBlock+154, new[0].name, sizeInBlocks);
    return new;
}

DirEntry *loadDir(DirEntry *directory){
    if(directory == NULL) {
        return NULL;
    }
    //Calculate space
    int sizeInBlocks = directory->sizeInBlocks;
    int startBlock =  directory->startBlock;
    void * buffer = malloc(sizeInBlocks * FS_BLOCKSIZE);
    if(buffer == NULL) return NULL;

    //Read to memory
    LBAread(buffer, sizeInBlocks, startBlock + vcb->dataStart);
    DirEntry *loadedDir = malloc(directory->size);
    if(loadedDir == NULL) return NULL;
    memcpy(loadedDir, buffer, directory->size);
    free(buffer);
    return loadedDir;
}

// two helper functions // 
// Normalize a path: collapse ".", "..", and extra "/".
// Input can be absolute ("/a/./b/../c").
// Output will be an absolute path starting with "/".
// Returns 0 on success, -1 on error.

static int normalizePath(const char *input, char *output, size_t outSize) {
    if (input == NULL || output == NULL || outSize == 0)
        return -1;

    char temp[MAX_PATH_LENGTH];
    size_t inLen = strlen(input);
    if (inLen >= sizeof(temp))
        return -1;

    strcpy(temp, input);

    char *segments[256];
    int depth = 0;

    char *savePtr = NULL;
    char *token = strtok_r(temp, "/", &savePtr);

    while (token != NULL) {
        if (strcmp(token, "") == 0 || strcmp(token, ".") == 0) {
            // skip
        } else if (strcmp(token, "..") == 0) {
            if (depth > 0)
                depth--;
        } else {
            if (depth >= 256)
                return -1;
            segments[depth++] = token;
        }
        token = strtok_r(NULL, "/", &savePtr);
    }

    // compute length
    size_t len = 1; // leading '/'
    for (int i = 0; i < depth; i++) {
        len += strlen(segments[i]);
        if (i < depth - 1)
            len++;
    }

    if (len + 1 > outSize)
        return -1;

    char *p = output;
    *p++ = '/';

    if (depth == 0) {
        *p = '\0';
        return 0;
    }

    for (int i = 0; i < depth; i++) {
        size_t segLen = strlen(segments[i]);
        memcpy(p, segments[i], segLen);
        p += segLen;
        if (i < depth - 1)
            *p++ = '/';
    }

    *p = '\0';
    return 0;
}

// Resolve an absolute, normalized path (starting with '/') into a DirEntry*
// using the existing parsePath logic. Returns NULL if the path doesn't exist
// or isn't a directory.
static DirEntry *resolveAbsolutePath(const char *absPath) {
    if (absPath == NULL || absPath[0] != '/')
        return NULL;

    // Root is a special case: "/" → Root
    if (strcmp(absPath, "/") == 0) {
        return Root;
    }

    // parsePath mutates its input, so work on a copy
    char *copy = strdup(absPath);
    if (copy == NULL) {
        return NULL;
    }

    PPinfo ppi;
    int rc = parsePath(copy, &ppi);
    free(copy);

    if (rc < 0) {
        freePPInfo(&ppi);
        return NULL;    // invalid path
    }

    // If lastElementName is NULL, that means the path itself is the parent
    if (ppi.lastElementName == NULL) {
        return ppi.parent;
    }

    
    int idx = ppi.index;
    if (idx < 0) {
        freePPInfo(&ppi);
        return NULL;
    }

    if (ppi.parent[idx].isDir == 0) {
        freePPInfo(&ppi);
        return NULL;   // not a directory
    }

    // Load the final directory into memory
    DirEntry *childDir = loadDir(&ppi.parent[idx]);
    if (childDir == NULL) {
        freePPInfo(&ppi);
        return NULL;
    }

    // If parsePath allocated an intermediate directory as parent,
    // free it here to avoid leaks (but never free Root or cwd).
    if (ppi.parent != Root && ppi.parent != cwd) {
        freePPInfo(&ppi);
    }

    return childDir;
}

//Opens a directory
//Returns the file descriptor
fdDir* fs_opendir(const char *pathname) {
    if(pathname == NULL) return NULL;
    
    //Allocate space
    fdDir *dirp = malloc(sizeof(fdDir));
    if (dirp == NULL) return NULL;
    memset(dirp, 0, sizeof(fdDir));
    dirp->di = malloc(sizeof(struct fs_diriteminfo));
    if (dirp->di == NULL) {
        free(dirp);
        return NULL;
    }

    //Parse path
    PPinfo ppInfo;
    int parseReturn = parsePath((char*)pathname, &ppInfo);
    DirEntry *targetDirEntry = findDirInDirectory(ppInfo.parent, ppInfo.lastElementName);
    if(targetDirEntry == NULL) {
        free(dirp->di);
        free(dirp);
        return NULL;
    }

    DirEntry *dirEntry = loadDir(targetDirEntry);
    if(targetDirEntry == NULL) {
        free(dirp->di);
        free(dirp);
        return NULL;
    }

    //Init fields
    dirp->directory = dirEntry;

    dirp->dirEntryPosition = 0;
    dirp->d_reclen = sizeof(DirEntry);
    dirp->totalEntries = dirEntry->size / sizeof(DirEntry);

    return dirp;
}
//Read Directory
//Returns the the directory into a struct
struct fs_diriteminfo* fs_readdir(fdDir *dirp) {
    if(dirp == NULL) return NULL;
    if(dirp->directory == NULL) return NULL;

    //Get current dir entries
    DirEntry *entries = (DirEntry*)dirp->directory;



    //Skip certain entries
    while (dirp->dirEntryPosition < dirp->totalEntries)
    {
        DirEntry *currentEntry = &entries[dirp->dirEntryPosition];
        if(currentEntry->isUsed == 1) {
            //Fill diriteminfo struct
            dirp->di->d_reclen = sizeof(DirEntry);

            //Set directory type
            if(currentEntry->isDir) {
                dirp->di->fileType = FT_DIRECTORY;
            } else {
                dirp->di->fileType = FT_REGFILE;
            }

            //Copy name
            strncpy(dirp->di->d_name, currentEntry->name, currentEntry->nameLength);
            dirp->di->d_name[currentEntry->nameLength] = '\0';
            dirp->dirEntryPosition++;
            return dirp->di;
        }
        dirp->dirEntryPosition++;
    }

    return NULL;
}

//Closes a directory
//Free memory here
int fs_closedir(fdDir *dirp) {
    if(dirp == NULL) {
        //printf("\nDirp Empty\n");
        return -1; 
    }

    //TODO possible memory leaks with names?
    //free(dirp->directory);
    //dirp->directory = NULL;
    if(dirp->di != NULL) {
        free(dirp->di);
        dirp->di = NULL;
    }
    free(dirp);
    dirp = NULL;
    return 0;
}

int parsePath(char *pathName, PPinfo *ppi){
    if(pathName == NULL) return -1;
    if(ppi == NULL) return -1;

    DirEntry * startParent = NULL;
    DirEntry * parent = NULL;
    char * savePtr = NULL;
    char * token1 = NULL;
    char * token2 = NULL;

    char *pathCopy = strdup(pathName);
    if(pathCopy == NULL) return -1;

    if(pathCopy[0] == '/') //absolute Path
        startParent = Root;
    else
        startParent = cwd;
    
    parent = startParent;

    token1 = strtok_r(pathCopy, "/", &savePtr);
    if(token1 == NULL)
    {
        if(parent == Root)
        {
            ppi->parent = parent;
            ppi->lastElementName = NULL;
            ppi->index = -2;
            free(pathCopy);
            return 0;
        }
        else
        {
            free(pathCopy);
            return -1;
        }
    }
    while(1)
    {
        int idx = findInDirectory(parent, token1);
        token2 = strtok_r(NULL, "/", &savePtr);
        if(token2 == NULL)
        {
            ppi->parent = parent;
            ppi->lastElementName = strdup(token1); //May need to do a strdup
            ppi->index = idx;
            free(pathCopy);
            return 0;
        }
        else
        {
            if(idx == -1)
            {
                free(pathCopy);
                return -2; //Invalid Path
            }
            if(parent[idx].isDir == 0)
            {
                free(pathCopy);
                return -3; //Invalid Path, is a file
            }
            DirEntry *tempParent = loadDir(&(parent[idx]));
            if(tempParent == NULL) {
                free(pathCopy);
                return -4;
            }
            if(parent != startParent)
                free(parent);
            parent = tempParent;
            token1 = token2;
        }
    } 
}

int findInDirectory(DirEntry *parent, char *token){ //min 56 in ParsePath lecture
    if(parent == NULL) return -1;
    DirEntry curr;
    int end = parent[1].size / sizeof(DirEntry);
    for(int i = 2; i < end; i++){
        curr = parent[i];
        if(strcmp(curr.name, token) == 0) return i;
    }
    return -1;
}

DirEntry *findDirInDirectory(DirEntry *parent, char *token){
    //temporary fix
    if(token==NULL){
        return Root;
    }
    int i = 0;
    int dirCount = parent[0].size / sizeof(DirEntry);
    for (int i = 0; i < dirCount; i++)
    {
        if(strcmp(parent[i].name, token) == 0) { 
            return &parent[i];
        }
    }
    return NULL;
}

void writeDirToDisk(DirEntry *newDir){
    if(newDir==NULL) return;
    return;
}

// fs_delete implementation
int fs_delete(char* fileName) {
    if (fileName == NULL) {
        return -1;
    }
    printf("\nDeleting File: %s \n", fileName);

    char *pathCopy = strdup(fileName);
    if (pathCopy == NULL) return -1;

    PPinfo ppInfo;
    int ret = parsePath(pathCopy, &ppInfo);
    
    // check if path parsing failed or file not found
    if (ret != 0 || ppInfo.index == -1) {
        printf("fs_delete: File '%s' not found.\n", fileName);
        free(pathCopy);
        if (ppInfo.lastElementName) free(ppInfo.lastElementName);
        if (ppInfo.parent != Root && ppInfo.parent != cwd) free(ppInfo.parent);
        return -1;
    }

    // get the pointer to the file entry
    DirEntry *targetEntry = &ppInfo.parent[ppInfo.index];

    // make sure its a file and not a directory
    if (targetEntry->isDir == 1) {
        printf("fs_delete: '%s' is a directory. Use rmdir.\n", fileName);
        free(pathCopy);
        if (ppInfo.lastElementName) free(ppInfo.lastElementName);
        if (ppInfo.parent != Root && ppInfo.parent != cwd) free(ppInfo.parent);
        return -1;
    }

    // free the data blocks used by the file
    if (targetEntry->startBlock != -1) {
        freeChain(targetEntry->startBlock, vcb);
    }

    // clear out the entry metadata in the directory
    targetEntry->name[0] = '\0'; // mark as unused
    targetEntry->startBlock = 0;
    targetEntry->size = 0;
    targetEntry->sizeInBlocks = 0;
    targetEntry->isDir = 0;

    // saveto disk
    int parentLBA = vcb->dataStart + ppInfo.parent[0].startBlock;
    LBAwrite(ppInfo.parent, ppInfo.parent[0].sizeInBlocks, parentLBA);

    free(pathCopy);
    if (ppInfo.lastElementName) free(ppInfo.lastElementName);
    if (ppInfo.parent != Root && ppInfo.parent != cwd) free(ppInfo.parent);

    return 0;
}

// fs_stat implementation
int fs_stat(const char *path, struct fs_stat *buf) {
    printf("\nfs_stat: %s\n", path);
    if(buf == NULL) return -1;
    if(path == NULL) return -1;
    PPinfo ppInfo;
    
    parsePath((char*)path, &ppInfo);

    int index = ppInfo.index;
    
    if(index < 0) {
        freePPInfo(&ppInfo);
        return -1;
    }

    DirEntry dir = ppInfo.parent[index];
    buf->st_size = dir.size;
    buf->st_blksize = FS_BLOCKSIZE;
    buf->st_blocks = dir.sizeInBlocks;
    buf->st_accesstime = 0;
    buf->st_createtime = 0;
    buf->st_createtime = 0;
    freePPInfo(&ppInfo);
    return 1;
}


// fs_rmdir implementation
int fs_rmdir(const char* pathname) {
    if (pathname == NULL) {
        return -1;
    }
   // open the directory so we can check whats inside
    fdDir *dirp = fs_opendir(pathname);
   
    if (dirp == NULL) {
        printf("fs_rmdir: Directory '%s' not found.\n", pathname);
        return -1;
    }

    
    int isEmpty = 1;
    struct fs_diriteminfo *di;

    
    fs_readdir(dirp); 
    fs_readdir(dirp); 

    // check if there is anything else in there
    if ((di = fs_readdir(dirp)) != NULL) {
        isEmpty = 0;
    }

    // save start block to free it after closing
    int targetBlock = dirp->directory[0].startBlock;

    // close it
    fs_closedir(dirp);

    // check if empty or not
    if (isEmpty == 0) {
        printf("fs_rmdir: Directory is not empty.\n");
        return -1;
    }

    // find and remove entry from parent directory (cwd)
    int numEntries = cwd[0].size / sizeof(DirEntry);
    int foundIndex = -1;
    int k = 0;

    while(k < numEntries) {
        if (cwd[k].isUsed == 1 && strcmp(cwd[k].name, pathname) == 0) {
            foundIndex = k;
            break;
        }
        k++;
    }

    if (foundIndex != -1) {
        // check that what we found is actually a dir
        if (cwd[foundIndex].isDir == 0) {
            printf("fs_rmdir: '%s' is not a directory.\n", pathname);
            return -1;
        }

        // clear the space used by the directory
        freeChain(targetBlock, vcb);

        // wipe the metadata from the parent
        cwd[foundIndex].isUsed = 0;
        cwd[foundIndex].name[0] = '\0'; 
        cwd[foundIndex].startBlock = 0;
        cwd[foundIndex].size = 0;
        cwd[foundIndex].isDir = 0;

        // save updates to disk
        int parentLBA = vcb->dataStart + cwd[0].startBlock;
        LBAwrite(cwd, cwd[0].sizeInBlocks, parentLBA);

        return 0;
    }

    return -1;
}

