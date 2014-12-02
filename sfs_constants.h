#ifndef __SFS_CONSTANTS_H
#define __SFS_CONSTANTS_H

#define BLOCK_SIZE 512
#define TOTAL_DATA_BLOCKS (BLOCK_SIZE * 8)
#define DIRECTORY_BLOCKS 100
#define DIR_START 1
#define DIR_BYTES (BLOCK_SIZE * DIRECTORY_BLOCKS)
#define FAT_START (DIR_START + DIRECTORY_BLOCKS)
#define FREE_LIST_START (FAT_START + FAT_BLOCKS)
#define FREE_LIST_LEN 1
#define NUM_BLOCKS (FREE_LIST_START + FREE_LIST_LEN + TOTAL_DATA_BLOCKS)
#define MAX_NAME_LEN 256
#define END_OF_FILE -1
#define NO_DATA -2

extern const int FAT_BYTES;
extern const int FAT_BLOCKS;
extern const int DATA_BLOCK_OFFSET;

#endif