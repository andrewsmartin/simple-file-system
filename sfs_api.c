#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "lib/disk_emu.h"
#include "sfs_api.h"
#include "sfs_types.h"
#include "free_block_list.h"

#define DISK_FILE "test.disk"

#define BLOCK_SIZE 512
#define TOTAL_DATA_BLOCKS (BLOCK_SIZE * 8)  // Comes from space available in bit vector block
#define DIRECTORY_BLOCKS 10 // Number of blocks in directory

typedef struct
{
    byte used;
    int data_block;
    int next;
} FatEntry;

#define FAT_BYTES (sizeof(FatEntry) * TOTAL_DATA_BLOCKS) // Number of fat entries should be equal to number of data blocks.
#define FAT_BLOCKS (FAT_BYTES / BLOCK_SIZE)
#define DATA_BLOCK_OFFSET (2 + DIRECTORY_BLOCKS + FAT_BLOCKS)
#define DIR_BYTES (BLOCK_SIZE * DIRECTORY_BLOCKS)

#define NUM_BLOCKS (DATA_BLOCK_OFFSET + TOTAL_DATA_BLOCKS)

#define DIR_START 1
#define FAT_START (DIR_START + DIRECTORY_BLOCKS)
#define FREE_LIST_START (FAT_START + FAT_BLOCKS)

#define MAX_NAME_LEN 256
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

static void flush_caches();

static void load_all_caches();
static void load_directory_from_buf(byte *buf);
static void load_fat_from_buf(byte *buf);

static void write_all_caches();
static void write_super_block();
static void write_directory();
static void write_fat_table();
static void write_free_block_list();

static FileDescriptor *get_file_desc(int fileID);
static int create_file(char *name);
static int alloc_block(uint16_t fat_index);
static int search_directory(char *name);
static int get_empty_dir_slot();
static int get_empty_fat_slot();
static int get_file_desc_slot();
static void init_caches();
static int create_file_desc(int dir_index);
static void remove_file_desc(int dir_index);
static int create_fat_entry(int dblock_i);
static int create_dir_entry(char *name, int fat_root);

static DirEntry *directory[NUM_DIR_ENTRIES];
static FatEntry *fat_table[TOTAL_DATA_BLOCKS];
static FileDescriptor *fdesc_table[MAX_OPEN];
static FreeBlockList *free_block_list;


/*** SCAFFOLDING: get rid of later. ***/
static void print_super_block();
static void print_fat();


void mksfs(int fresh)
{
    init_caches();
    if (fresh) {
        init_fresh_disk(DISK_FILE, BLOCK_SIZE, NUM_BLOCKS);
        flush_caches();
    }
    else {
        init_disk(DISK_FILE, BLOCK_SIZE, NUM_BLOCKS);
        load_all_caches();
    }
}

void sfs_ls()
{
    int i;
    for (i = 0; i < NUM_DIR_ENTRIES; i++) {
        DirEntry *f = directory[i];
        if (f != NULL) {
            printf("%s, %ld\n", f->name, f->size);
        }
    }
}

int sfs_fopen(char *name)
{
    int dir_index = search_directory(name);
    if (dir_index == -1) {
        dir_index = create_file(name);
        if (dir_index == -1) {
            puts("Could not create file.");
            return -1;
        }
    }

    int fileID = create_file_desc(dir_index);
    
    return fileID;
}

void sfs_fclose(int fileID)
{
    if (fileID >= MAX_OPEN) {
        printf("Error: id [%d] is outside range of allowed values.\n", fileID);
        return;
    }

    remove_file_desc(fileID);
}

void sfs_fwrite(int fileID, char *buf, int length)
{
    FileDescriptor *f = get_file_desc(fileID);
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }

    int fat_index = f->fat_root;

    int bytes_left = length;
    byte *ptr = (byte*) buf;
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
        f->write_ptr.block_address = fat_table[f->write_ptr.block_address]->next;
        bytes_left -= fill;
    }

    // Try this differently. Allocate blocks one at a time as needed.
    int i;
    for (i = 0; i < bytes_left / BLOCK_SIZE; i++, ptr += BLOCK_SIZE) {
        // Get start address of next block.
        if (f->write_ptr.block_address == NO_DATA) {
            if (alloc_block(fat_index) != 0) {
                puts("Could not allocate block. Not writing further data.");
                return;
            }
            f->write_ptr.block_address = fat_table[fat_index]->data_block;
        }

        write_blocks(f->write_ptr.block_address, 1, ptr);
        bytes_left -= BLOCK_SIZE;

        if (fat_table[fat_index]->next == END_OF_FILE && bytes_left > 0) {
            fat_table[fat_index]->next = create_fat_entry(NO_DATA);
            if (fat_index < 0) {
                puts("Could not allocate new fat entry. Aborting write.");
                return;
            }
        }
        fat_index = fat_table[fat_index]->next;
        f->write_ptr.block_address = fat_table[fat_index]->data_block;
    }

    // Now write the remaining bytes in the next block...
    if (bytes_left > 0) {
        if (alloc_block(fat_index) != 0) {
            puts("Could not allocate block. Not writing further data.");
            return;
        }

        f->write_ptr.block_address = fat_table[fat_index]->data_block; 
        byte tmp[BLOCK_SIZE];
        memcpy(tmp, ptr, bytes_left);
        write_blocks(f->write_ptr.block_address, 1, tmp);
        f->write_ptr.byte_address = bytes_left;
    }
    
    // Somehow update the file size (i.e. the directory entry)
}

void sfs_fread(int fileID, char *buf, int length)
{
    FileDescriptor *f = get_file_desc(fileID);
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }

    byte *ptr = (byte*) buf;
    int n, fat_index = f->fat_root;

    // First read rest of remaining block if byte address > 0;
    if (f->read_ptr.byte_address > 0) {
        int fill = BLOCK_SIZE - f->read_ptr.byte_address;
        byte tmp[BLOCK_SIZE];
        read_blocks(f->read_ptr.block_address, 1, tmp);
        memcpy(buf, tmp + f->read_ptr.byte_address, fill);
        f->read_ptr.byte_address = 0;
        length -= fill;
        ptr += fill;
    }

    for (n = 0; n < (length / BLOCK_SIZE); n++, ptr += BLOCK_SIZE) {
        read_blocks(f->read_ptr.block_address, 1, ptr);
        // Set the read pointer to the next block in the chain.
        fat_index = fat_table[fat_index]->next;
        if (fat_index == END_OF_FILE) {
            return;
        }
        f->read_ptr.block_address = fat_table[fat_index]->data_block;
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
    int fat_index = create_fat_entry(NO_DATA);
    if (fat_index == -1) return -1;
    int dir_index = create_dir_entry(name, fat_index);
    if (dir_index == -1) return -1;
    
    flush_caches();
    return dir_index;
}

int alloc_block(uint16_t fat_index)
{
    // Find a free data block from the free list.
    int db = fbl_get_free_index(free_block_list);

    if (db < 0) {
        puts("There are no free blocks available.");
        return -1;
    }

    fat_table[fat_index]->data_block = db + DATA_BLOCK_OFFSET;

    flush_caches();
    return 0;
}

/* An important function. When called, the cached super block, directory
   FAT and free block list are synchronized and flushed to disk. */
void flush_caches()
{
    super_block.num_free_blocks = fbl_get_num_free(free_block_list);
    //print_super_block();
    //print_fat();
    write_all_caches();
}

void load_all_caches()
{
    byte buf[DATA_BLOCK_OFFSET * BLOCK_SIZE], *ptr = buf;
    read_blocks(0, DATA_BLOCK_OFFSET, buf);
    memcpy(&super_block, ptr, BLOCK_SIZE);
    print_super_block();
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
    for (i = 0; i < NUM_DIR_ENTRIES; i++) {
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

int create_file_desc(int dir_index)
{
    int i = get_file_desc_slot();
    if (i == -1) {
        puts("Maximum allowed open files. Please close some files before attempting to open.");
        return -1;
    }

    int fat_index = directory[dir_index]->fat_index;

    FileDescriptor *desc = malloc(sizeof(FileDescriptor));
    desc->fat_root = fat_index;
    desc->read_ptr.block_address = fat_table[fat_index]->data_block;
    desc->read_ptr.byte_address = 0;
    desc->write_ptr.block_address = fat_table[fat_index]->data_block;
    desc->write_ptr.byte_address = 0;

    fdesc_table[i] = desc;

    return i;
}

void remove_file_desc(int fileID)
{
    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) {
        printf("Error: no open file with id [%d].\n", fileID);
        return;
    }
    free(f);
    fdesc_table[fileID] = NULL;
}

int create_fat_entry(int dblock_i)
{
    int new_index = get_empty_fat_slot();
    if (new_index < 0) {
        puts("There are no open slots in the file allocation table.");
        return -1;
    }
    FatEntry *fat = malloc(sizeof(FatEntry));
    fat->used = 1;
    fat->data_block = dblock_i;
    fat->next = END_OF_FILE;
    fat_table[new_index] = fat;

    return new_index;
}

int create_dir_entry(char *name, int fat_root)
{
    int i = get_empty_dir_slot();
    if (i < 0) {
        puts("Too many files in directory.");
        return -1;
    }

    DirEntry *new_file = malloc(sizeof(DirEntry));
    new_file->used = 1;
    strncpy(new_file->name, name, MAX_NAME_LEN);
    new_file->size = 0;
    new_file->fat_index = fat_root;
    directory[i] = new_file;

    return i;
}

FileDescriptor *get_file_desc(int fileID)
{
    if (fileID >= MAX_OPEN) {
        printf("Error: id [%d] is outside range of allowed values.\n", fileID);
        return NULL;
    }

    return fdesc_table[fileID];
}

/*** Scaffolding. Get rid of when finished. ***/
void print_super_block()
{
    printf("%d, %d, %d, %d, %d\n", super_block.block_size, super_block.num_blocks_root,
        super_block.num_blocks_fat, super_block.num_data_blocks, super_block.num_free_blocks);
}

void print_fat()
{
    int i;
    for (i = 0; i < TOTAL_DATA_BLOCKS; i++) {
        FatEntry *f = fat_table[i];
        if (f == NULL) continue;
        printf("Fat entry[%d] -- Used:%d Data block index:%d Next fat index:%d\n",
            i, f->used, f->data_block, f->next);
    }
}