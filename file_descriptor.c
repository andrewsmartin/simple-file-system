#include "file_descriptor.h"
#include "sfs_types.h"
#include "sfs_constants.h"
#include "dir_cache.h"
#include "fat_cache.h"

#include "lib/disk_emu.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MIN(a, b) (a < b ? a : b)
#define MAX_OPEN 1000

typedef struct
{

    int curr_fat;
    uint16_t byte_address;
} FilePtr;

typedef struct 
{
    uint16_t fat_root;
    char name[MAX_NAME_LEN];
    FilePtr read_ptr, write_ptr;    
} FileDescriptor;

static void write_data_block(FileDescriptor *f, byte **ptr, int length);
static void read_data_block(FileDescriptor *f, byte **ptr, int length);

static FileDescriptor *fdesc_table[MAX_OPEN];

int fdesc_search(char *name)
{
    int i;
    for (i = 0; i < MAX_OPEN; i++) {
        if (fdesc_table[i] == NULL) {
            continue;
        }
        if (strncmp(name, fdesc_table[i]->name, MAX_NAME_LEN) == 0) {
            return i;
        }
    }

    return ERR_NOT_FOUND;
}

int fdesc_create(int dir_index)
{
    int i;
    for (i = 0; i < MAX_OPEN; i++) {
        if (fdesc_table[i] == NULL) break;
    }

    if (i == MAX_OPEN) return ERR_MAX_OPEN;

    int fat_index = dir_get_fat_root(dir_index);

    FileDescriptor *desc = malloc(sizeof(FileDescriptor));
    strncpy(desc->name, dir_get_name(dir_index), MAX_NAME_LEN);
    desc->fat_root = fat_index;
    desc->read_ptr.curr_fat = fat_index;
    desc->read_ptr.byte_address = 0;
    desc->write_ptr.curr_fat = fat_get_tail(fat_index);
    desc->write_ptr.byte_address = dir_get_size(dir_index) % BLOCK_SIZE;

    fdesc_table[i] = desc;

    return i;
}

int fdesc_remove(int fileID)
{
    if (fileID >= MAX_OPEN || fileID < 0) return ERR_NOT_FOUND;
    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) return ERR_NOT_FOUND;
    free(f);
    fdesc_table[fileID] = NULL;
    return 0;
}

int fdesc_write(int fileID, char *buf, int length)
{
    if (fileID >= MAX_OPEN || fileID < 0) return ERR_NOT_FOUND;
    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) return ERR_NOT_FOUND;

    int curr_fat = f->write_ptr.curr_fat;
    int curr_db = fat_get_data_block(curr_fat);

    byte *ptr = (byte*) buf;
    int num_init_write = 0;
    /* If write pointer is in the middle of a block, fill up
       the block first. We need to treat this case specially because
       we have to read existing data. */
    if (f->write_ptr.byte_address > 0) {
        byte tmp[BLOCK_SIZE] = {0};
        read_blocks(curr_db, 1, tmp);
        int fill = BLOCK_SIZE - f->write_ptr.byte_address;
        num_init_write = MIN(length, fill);
        memcpy(tmp + f->write_ptr.byte_address, ptr, num_init_write);
        ptr += num_init_write;
        write_blocks(curr_db, 1, tmp);
        f->write_ptr.byte_address += num_init_write;
    }

    int bytes_left = length - num_init_write;

    while (bytes_left > 0) {
        if (f->write_ptr.byte_address == BLOCK_SIZE) {
            f->write_ptr.byte_address = 0;
            int next;
            // Prepare next fat entry if there is still data to write.
            if (fat_get_next_index(f->write_ptr.curr_fat) == END_OF_FILE && bytes_left > 0) {
                next = fat_create_entry();
                if (next == ERR_OUT_OF_SPACE) {
                    puts("Could not allocate new fat entry. Not writing further data.");
                    goto updateSize;
                }
                fat_set_next_index(f->write_ptr.curr_fat, next);
            }
            f->write_ptr.curr_fat = next;
            curr_db = fat_get_data_block(f->write_ptr.curr_fat);
        }

        if (curr_db == NO_DATA) {
            if (fat_alloc_block(f->write_ptr.curr_fat) == ERR_OUT_OF_SPACE) {
                puts("Could not allocate block. Not writing further data.");
                goto updateSize;
            }
        }
        
        int bytes = MIN(bytes_left, BLOCK_SIZE);
        write_data_block(f, &ptr, bytes);
        bytes_left -= bytes;
    }

    goto updateSize;
updateSize:
    dir_inc_size(dir_search(f->name), length - bytes_left);
    return 0;
}

int fdesc_read(int fileID, char *buf, int length)
{
    if (fileID >= MAX_OPEN || fileID < 0) return ERR_NOT_FOUND;
    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) return ERR_NOT_FOUND;

    byte *ptr = (byte*) buf;
    int bytes_left = length;

    while (bytes_left > 0) {
        if (f->read_ptr.byte_address == BLOCK_SIZE) {
            f->read_ptr.byte_address = 0;

            if (fat_get_next_index(f->read_ptr.curr_fat) == END_OF_FILE) {
                return ERR_UNKNOWN;
            }

            f->read_ptr.curr_fat = fat_get_next_index(f->read_ptr.curr_fat);
        }
        
        if (f->read_ptr.curr_fat == END_OF_FILE || fat_get_data_block(f->read_ptr.curr_fat) == NO_DATA) {
            return ERR_UNKNOWN;
        }

        int bytes = MIN(bytes_left, BLOCK_SIZE);
        if (bytes > BLOCK_SIZE - f->read_ptr.byte_address) {
            bytes = BLOCK_SIZE - f->read_ptr.byte_address;
        }
        read_data_block(f, &ptr, bytes);
        bytes_left -= bytes;
    }

    return 0;
}

int fdesc_seek(int fileID, int loc)
{
    if (fileID >= MAX_OPEN || fileID < 0) return ERR_NOT_FOUND;
    FileDescriptor *f = fdesc_table[fileID];
    if (f == NULL) return ERR_NOT_FOUND;

    int fat_index = f->fat_root;
    int num_blocks = loc / BLOCK_SIZE, i;

    for (i = 0; (i < num_blocks && fat_index != END_OF_FILE); i++) {
        fat_index = fat_get_next_index(fat_index);
    }

    f->read_ptr.curr_fat = fat_index;
    f->write_ptr.curr_fat = fat_index;

    f->read_ptr.byte_address = (loc % BLOCK_SIZE);
    f->write_ptr.byte_address = (loc % BLOCK_SIZE);
    return 0;
}

void write_data_block(FileDescriptor *f, byte **ptr, int length)
{
    byte tmp[BLOCK_SIZE] = {0};
    memcpy(tmp, *ptr, length);
    int db = fat_get_data_block(f->write_ptr.curr_fat);
    write_blocks(db, 1, *ptr);
    f->write_ptr.byte_address += length;
    (*ptr) += length;
}

void read_data_block(FileDescriptor *f, byte **ptr, int length)
{
    byte tmp[BLOCK_SIZE] = {0};
    int db = fat_get_data_block(f->read_ptr.curr_fat);
    read_blocks(db, 1, tmp);
    memcpy(*ptr, tmp + f->read_ptr.byte_address, length);
    f->read_ptr.byte_address += length;
    (*ptr) += length;
}