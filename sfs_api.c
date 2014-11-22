#include "sfs_api.h"
#include "disk_emu.h"

#define BLOCK_SIZE 512      // Physical block size
#define NUM_BLOCKS 10000    // Number of physical blocks
#define DIRECTORY_BLOCKS 10 // Number of blocks in directory
#define FAT_BLOCKS 10       // Number of blocks in file allocation table

typedef struct
{
    // Maps file names to FAT entries
} directory;

struct
{
    const int block_size = BLOCK_SIZE;
    const int num_blocks_root = DIRECTORY_BLOCKS;
    const int num_blocks_fat = FAT_BLOCKS;
    int num_data_blocks;
    int num_free_blocks;
} super_block;

typedef struct 
{
    int fat_root;   // FAT index of the root data block
    void *write_ptr, *read_ptr;    
} FileDescriptor;

int free_block_list[BLOCK_SIZE / sizeof(int)];
char **directory; // An array mapping a block number to a file name


void mksfs(int fresh)
{
    // Initialize the file system by creating a file (used to emulate the disk).
    char *filename = ".disk";
    if (fresh) {
        init_fresh_disk(filename, BLOCK_SIZE, NUM_BLOCKS);
        super_block.num_blocks = NUM_BLOCKS;
        super_block.num_free_blocks = BLOCK_SIZE - 1;
        super_block.num_free_fcb = 0;   // TODO: use correct number
        void *block_buf = calloc(BLOCK_SIZE, 1);
        memcpy(block_buf, &super_block, sizeof(struct super_block));
        write_blocks(0, 1, block_buf);
    }
    else {
        init_disk(filename, BLOCK_SIZE, NUM_BLOCKS);
    }
}

void sfs_ls()
{

}

int sfs_fopen(char *name)
{
    // Look up entry in directory. If not found, create the file.
}

void sfs_fclose(int fileID)
{

}

void sfs_fwrite(int fileID, char *buf, int length)
{

}

void sfs_fread(int fileID, char *buf, int length)
{

}

void sfs_fseek(int fileID, int loc)
{

}

int sfs_remove(char *file)
{

}