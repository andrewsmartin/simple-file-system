#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "lib/disk_emu.h"
#include "sfs_api.h"
#include "sfs_types.h"
#include "free_block_list.h"

#define DISK_FILE "test.disk"

#define BLOCK_SIZE 512      // Physical block size (bytes)
/* Number of physical blocks avaiable for file data. This limit comes from the 
   fact that we are using a bit vector to store the list of free blocks, and
   we are assuming that this will occupy only a single block on the disk. */
#define TOTAL_DATA_BLOCKS (BLOCK_SIZE * 8)
#define DIRECTORY_BLOCKS 10 // Number of blocks in directory

typedef struct
{
    byte used;
    int data_block;
    int next;
} FatEntry;

#define FAT_BYTES (sizeof(FatEntry) * TOTAL_DATA_BLOCKS) // Number of fat entries should be equal to number of data blocks.
#define FAT_BLOCKS (FAT_BYTES / BLOCK_SIZE)
#define NUM_META_BLOCKS (2 + DIRECTORY_BLOCKS + FAT_BLOCKS)
#define DIR_BYTES (BLOCK_SIZE * DIRECTORY_BLOCKS)

// Total number of physical blocks, including those reserved for on-disk data structures
#define NUM_BLOCKS (NUM_META_BLOCKS + TOTAL_DATA_BLOCKS)

#define DIR_START 1
#define FAT_START (DIR_START + DIRECTORY_BLOCKS)
#define FREE_LIST_START (FAT_START + FAT_BLOCKS)

#define MAX_NAME_LEN 256
#define ATTRS_LEN 0
#define NUM_DIR_ENTRIES (DIR_BYTES / sizeof(DirEntry))
#define MAX_OPEN 1000
#define END_OF_FILE -1
#define NO_DATA -2

static struct
{
    const uint16_t block_size;
    const uint16_t num_blocks_root;
    const uint16_t num_blocks_fat;
    const uint32_t num_data_blocks;
    uint32_t num_free_blocks;
} super_block = {.block_size = BLOCK_SIZE, .num_blocks_root = DIRECTORY_BLOCKS, 
    .num_blocks_fat = FAT_BLOCKS, .num_data_blocks = TOTAL_DATA_BLOCKS};

typedef struct
{
    byte used;
    char name[MAX_NAME_LEN];
    long size;
    uint16_t fat_index;
} DirEntry;

typedef struct
{
    uint32_t block_address;
    uint16_t byte_address;
} FilePtr;

typedef struct 
{
    uint16_t fat_root;
    FilePtr read_ptr, write_ptr;    
} FileDescriptor;

static void load_all_caches();
static void load_directory_from_buf(byte *buf);
static void load_fat_from_buf(byte *buf);

static void write_all_caches();
static void write_super_block();
static void write_directory();
static void write_fat_table();
static void write_free_block_list();

static int create_file(char *name);
static int alloc_blocks(uint16_t fat_index, int num_blocks);
static int search_directory(char *name);
static int get_empty_dir_slot();
static int get_empty_fat_slot();
static int get_file_desc_slot();
static void init_caches();

static DirEntry *directory[NUM_DIR_ENTRIES];
static FatEntry *fat_table[TOTAL_DATA_BLOCKS];
static FileDescriptor *fdesc_table[MAX_OPEN];
static FreeBlockList *free_block_list;


/*** SCAFFOLDING: get rid of later. ***/
static void print_super_block();


void mksfs(int fresh)
{
    init_caches();
    if (fresh) {
        init_fresh_disk(DISK_FILE, BLOCK_SIZE, NUM_BLOCKS);
        write_all_caches();
    }
    else {
        init_disk(DISK_FILE, BLOCK_SIZE, NUM_BLOCKS);
        load_all_caches();
    }
}

void sfs_ls()
{

}

int sfs_fopen(char *name)
{
    int fat_index;
    // TODO: lookup to see if exists first...
    int dir_index = search_directory(name);
    if (dir_index == -1) {
        fat_index = create_file(name);
    }
    else if (dir_index == -2) {
        puts("Cannot create file because directory is full.");
        return -1;
    }
    else {
        fat_index = directory[dir_index]->fat_index;
    }

    // Add an entry to the file descriptor table
    FileDescriptor *desc = malloc(sizeof(FileDescriptor));
    desc->fat_root = fat_index;
    desc->read_ptr.block_address = END_OF_FILE;
    desc->read_ptr.byte_address = 0;
    desc->write_ptr.block_address = END_OF_FILE;
    desc->write_ptr.byte_address = 0;
    int i = get_file_desc_slot();
    if (i == -1) {
        puts("Maximum allowed open files. Please close some files before attempting to open.");
        goto cleanupFile;
    }
    fdesc_table[i] = desc;
    
    return 0;
cleanupFile:
    // Remove from directory, FAT, etc
    free(desc);
    return -1;
}

void sfs_fclose(int fileID)
{
    if (fileID >= MAX_OPEN) {
        printf("Error: id [%d] is outside range of allowed values.\n", fileID);
        return;
    }

    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }

    free(f);
    fdesc_table[fileID] = NULL;
}

void sfs_fwrite(int fileID, char *buf, int length)
{
    if (fileID >= MAX_OPEN) {
        printf("Error: id [%d] is outside range of allowed values.\n", fileID);
        return;
    }

    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }

    byte *ptr = (byte*) buf;
    uint16_t last_fat_index = f->fat_root;
    // If write pointer is in the middle of a block, fill up
    // the block first.
    if (f->write_ptr.byte_address > 0) {
        byte tmp[BLOCK_SIZE];
        read_blocks(f->write_ptr.block_address, 1, tmp);
        int fill = BLOCK_SIZE - f->write_ptr.byte_address;
        memcpy(tmp + f->write_ptr.byte_address, ptr, fill);
        ptr += fill;
        write_blocks(f->write_ptr.block_address, 1, tmp);
        f->write_ptr.byte_address = 0;
        last_fat_index = f->write_ptr.block_address;
        f->write_ptr.block_address = fat_table[f->write_ptr.block_address]->next;
        length -= fill;
    }

    // Figure out how many blocks need to be allocated to complete the write.
    int num_blocks = length / BLOCK_SIZE, to_alloc, i;
    for (i = 0; i < num_blocks; i++) {
        if (f->write_ptr.block_address == END_OF_FILE) {
            to_alloc = num_blocks - i;
            break;
        }
        last_fat_index = f->write_ptr.block_address;
        f->write_ptr.block_address = fat_table[f->write_ptr.block_address]->next;
    }

    int not_alloc = alloc_blocks(last_fat_index, to_alloc);

    if (not_alloc > 0) {
        puts("Not enough space to write all data...writing until EOF");
    }

    // Now write the rest of the buffer.
    f->write_ptr.block_address = fat_table[f->write_ptr.block_address]->next;
    for (i = 0; i < num_blocks - not_alloc; i++, ptr += BLOCK_SIZE) {
        if (f->write_ptr.block_address == END_OF_FILE) {
            // We already failed at allocating...give up.
            break;
        }
        byte buffer[BLOCK_SIZE];
        read_blocks(f->write_ptr.block_address, 1, buffer);
        int to_write = length > BLOCK_SIZE ? length : BLOCK_SIZE;
        memcpy(buffer, ptr, to_write);
        write_blocks(f->write_ptr.block_address, 1, buffer);
        f->write_ptr.byte_address = (f->write_ptr.byte_address + to_write) % BLOCK_SIZE;
    }
}

void sfs_fread(int fileID, char *buf, int length)
{
    if (fileID >= MAX_OPEN) {
        printf("Error: id [%d] is outside range of allowed values.\n", fileID);
        return;
    }

    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }

    byte buffer[length], *ptr = buffer;
    int n;

    // First read rest of remaining block if byte address > 0;
    if (f->read_ptr.byte_address > 0) {
        int fill = BLOCK_SIZE - f->read_ptr.byte_address;
        byte tmp[BLOCK_SIZE];
        read_blocks(f->read_ptr.block_address, 1, tmp);
        memcpy(buffer, tmp + f->read_ptr.byte_address, fill);
        f->read_ptr.byte_address = 0;
        length -= fill;
    }

    for (n = 0; n < (length / BLOCK_SIZE); n++, ptr += BLOCK_SIZE) {
        read_blocks(f->read_ptr.block_address, 1, ptr);
        // Set the read pointer to the next block in the chain.
        int next_block = fat_table[f->read_ptr.block_address]->next;
        f->read_ptr.block_address = next_block;
        if (next_block == END_OF_FILE) {
            f->read_ptr.byte_address = 0;
            return;
        }
    }

    // Now read only part of the next block...
    int rem = length % BLOCK_SIZE;
    byte tmp[BLOCK_SIZE];
    read_blocks(f->read_ptr.block_address, 1, tmp);
    memcpy(ptr, tmp, rem);
    f->read_ptr.byte_address = rem;
}

void sfs_fseek(int fileID, int loc)
{
    if (fileID >= MAX_OPEN) {
        printf("Error: id [%d] is outside range of allowed values.\n", fileID);
        return;
    }

    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }

    int num_blocks = loc / BLOCK_SIZE, i;
    for (i = 0; i < num_blocks; i++) {
        int next_block = fat_table[f->read_ptr.block_address]->next;
        f->read_ptr.block_address = next_block;
        f->write_ptr.block_address = next_block;
         if (next_block == END_OF_FILE) {
            f->read_ptr.byte_address = 0;
            f->read_ptr.byte_address = 0;
            return;
        }
    }

    f->read_ptr.byte_address = (loc % BLOCK_SIZE);
    f->write_ptr.byte_address = (loc % BLOCK_SIZE);
}

int sfs_remove(char *file)
{
    return 0;
}

/*** PRIVATE HELPER FUNCTIONS ***/

/**
 * Adds a new file to the directory cache and writes the cache to disk.
 * If there are no slots left in the directory table or there are no free
 * blocks on the disk, and error message is thrown and the create is aborted.
*/
int create_file(char *name)
{
    int fat_index = get_empty_fat_slot();
    if (fat_index == -1) {
        puts("No empty slots in the file allocation table.");
        return -1;
    }

    FatEntry *fat = malloc(sizeof(FatEntry));
    fat->used = 1;
    fat->data_block = NO_DATA;
    fat->next = END_OF_FILE;
    fat_table[fat_index] = fat;

    // Add a new entry to the directory and write to disk.
    int i = get_empty_dir_slot();
    if (i == -1) {
        puts("Directory is full; cannot create new file. Delete one or more files and try again.");
        goto cleanupFatEntry;
    }
    DirEntry *new_file = malloc(sizeof(DirEntry));
    strncpy(new_file->name, name, MAX_NAME_LEN);
    new_file->size = 0;
    new_file->fat_index = fat_index;
    directory[i] = new_file;
    write_all_caches();
    return fat_index;
cleanupFatEntry:
    free(fat);
    return -1;
}

int alloc_blocks(uint16_t fat_index, int num_blocks)
{
    int i;
    for (i = 0; i < num_blocks; i++) {
        // Find a free data block from the free block list.
        uint32_t index = fbl_get_free_index(free_block_list);
        if (index < 0) {
            return num_blocks - i;
        }

        int new_index = get_empty_fat_slot();
        if (new_index < 0) {
            return num_blocks - i;
        }
        fbl_set_next_used(free_block_list);
        // Create a new entry in the fat at the end of the list
        FatEntry *fat = malloc(sizeof(FatEntry));
        fat->used = 1;
        fat->data_block = NO_DATA;
        fat->next = END_OF_FILE;
        fat_table[new_index] = fat;
        fat_table[fat_index]->next = new_index;
        fat_index = new_index;
    }

    return 0;
}

void load_all_caches()
{
    byte buf[NUM_META_BLOCKS * BLOCK_SIZE], *ptr = buf;
    read_blocks(0, NUM_META_BLOCKS, buf);
    memcpy(&super_block, ptr, BLOCK_SIZE);
    ptr += BLOCK_SIZE;
    byte dir_buf[DIR_BYTES];
    memcpy(dir_buf, ptr, DIR_BYTES);
    load_directory_from_buf(dir_buf);
    ptr += DIR_BYTES;
    byte fat_buf[FAT_BYTES];
    memcpy(fat_buf, ptr, FAT_BYTES);
    load_fat_from_buf(fat_buf);
    ptr += FAT_BYTES;
    free_block_list = fbl_create(TOTAL_DATA_BLOCKS);
    byte free_buf[BLOCK_SIZE];
    memcpy(free_buf, ptr, BLOCK_SIZE);
    fbl_set_raw(free_block_list, free_buf);
}

void load_directory_from_buf(byte *buf)
{
    int i, len = sizeof(DirEntry);
    byte *ptr = buf;
    for (i = 0; i < DIR_BYTES; i += len, ptr += len)
    {
        // First byte tells us whether or not this slot is used.
        if (buf[i] == 1) {
            DirEntry *dir_entry = malloc(len);
            memcpy(dir_entry, ptr, len);
            directory[i / len] = dir_entry;
        }
    }
}

void load_fat_from_buf(byte *buf)
{
    int i, len = sizeof(FatEntry);
    byte *ptr = buf;
    for (i = 0; i < FAT_BYTES; i += len, ptr += len)
    {
        // First byte tells us whether or not this slot is used.
        if (buf[i] == 1) {
            FatEntry *fat_entry = malloc(len);
            memcpy(fat_entry, ptr, len);
            fat_table[i / len] = fat_entry;
        }
    }
}

void load_super_block()
{
    read_blocks(0, 1, &super_block);
}

/** 
 * Updates the on-disk data structures to be synchronized with the cached data.
 * TODO: This could easily be optimized by combining each cache into a single
 * buffer and performing only one write.
*/
void write_all_caches()
{
    write_super_block();
    write_directory();
    write_fat_table();
    write_free_block_list();
}

void write_super_block()
{
    write_blocks(0, 1, &super_block);
}

void write_directory()
{
    byte dir_buf[DIR_BYTES] = {0}, *ptr = dir_buf;
    int i;
    for (i = 0; i < NUM_DIR_ENTRIES; i++, ptr += sizeof(DirEntry)) {
        DirEntry *d = directory[i];
        if (d != NULL)
            memcpy(ptr, d, sizeof(DirEntry));
    }

    write_blocks(DIR_START, DIRECTORY_BLOCKS, dir_buf);
}

void write_fat_table()
{
    byte buf[FAT_BYTES] = {0}, *ptr = buf;
    int i;
    for (i = 0; i < TOTAL_DATA_BLOCKS; i++, ptr += sizeof(FatEntry)) {
        FatEntry *f = fat_table[i];
        if (f != NULL)
            memcpy(ptr, f, sizeof(FatEntry));
    }

    write_blocks(FAT_START, FAT_BLOCKS, buf);
}

void write_free_block_list()
{
    write_blocks(FREE_LIST_START, 1, fbl_get_raw(free_block_list));
}

int search_directory(char *name)
{
    int i = 0, full = 1;
    for (i = 0; i < NUM_DIR_ENTRIES; i++)
    {
        if (directory[i] == NULL) {
            full = 0;
            continue;
        }
        if (strncmp(name, directory[i]->name, MAX_NAME_LEN) == 0) {
            return directory[i]->fat_index;
        }
    }

    return full ? -2 : -1;
}

int get_empty_dir_slot()
{
    int i = 0;
    for (i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (directory[i] == NULL) {
            return i;
        }
    }

    return -1;
}

int get_empty_fat_slot()
{
    int i = 0;
    for (i = 0; i < TOTAL_DATA_BLOCKS; i++) {
        if (fat_table[i] == NULL) {
            return i;
        }
    }

    return -1;
}

int get_file_desc_slot()
{
    int i = 0;
    for (i = 0; i < MAX_OPEN; i++) {
        if (fdesc_table[i] == NULL) {
            return i;
        }
    }

    return -1;
}

void init_caches()
{
    super_block.num_free_blocks = TOTAL_DATA_BLOCKS;
    free_block_list = fbl_create(TOTAL_DATA_BLOCKS);
}

/*** Scaffolding. Get rid of when finished. ***/
void print_super_block()
{
    printf("%d, %d, %d, %d, %d\n", super_block.block_size, super_block.num_blocks_root,
        super_block.num_blocks_fat, super_block.num_data_blocks, super_block.num_free_blocks);
}