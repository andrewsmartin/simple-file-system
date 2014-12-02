#include "dir_cache.h"
#include "sfs_types.h"
#include "sfs_constants.h"
#include "fat_cache.h"

#include "lib/disk_emu.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    byte used;
    char name[MAX_NAME_LEN];
    long size;
    uint16_t fat_index;
} DirEntry;

static int NUM_DIR_ENTRIES = DIR_BYTES / sizeof(DirEntry);
static DirEntry *directory[DIR_BYTES / sizeof(DirEntry)];
static DirEntry *iter;
static int curr_iter;

void dir_load()
{
    const int len = sizeof(DirEntry);
	int i;
    byte buf[DIR_BYTES], *ptr = buf;
    read_blocks(DIR_START, DIRECTORY_BLOCKS, buf);
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

void dir_flush()
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

void dir_iter_begin()
{
    // Set the current pointer to the first non-null entry.
    for (curr_iter = 0; curr_iter < NUM_DIR_ENTRIES; curr_iter++) {
        iter = directory[curr_iter];
        if (iter != NULL) break;
    }
}

int dir_curr_iter()
{
    return curr_iter;
}

int dir_iter_done()
{
    return curr_iter == NUM_DIR_ENTRIES ? 1 : 0;
}

void dir_iter_next()
{
    for (++curr_iter; curr_iter < NUM_DIR_ENTRIES; curr_iter++) {
        iter = directory[curr_iter];
        if (iter != NULL) break;
    }
}

int dir_search(char *name)
{
    int i;
    for (i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (directory[i] == NULL) 
            continue;
        else if (strncmp(name, directory[i]->name, MAX_NAME_LEN) == 0)
            return i;
    }

    return ERR_NOT_FOUND;
}

int dir_create_entry(char *name)
{
    int f_index = fat_create_entry();
    if (f_index == ERR_OUT_OF_SPACE)
        return ERR_OUT_OF_SPACE;
    int i;
    for (i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (directory[i] == NULL) {
            directory[i] = malloc(sizeof(DirEntry));
            directory[i]->used = 1;
            strncpy(directory[i]->name, name, MAX_NAME_LEN);
            directory[i]->size = 0;
            directory[i]->fat_index = fat_create_entry();
            return i;
        }
    }

    return ERR_OUT_OF_SPACE;
}

int dir_get_fat_root(int dir_index)
{
    return directory[dir_index]->fat_index;
}

char *dir_get_name(int dir_index)
{
    return directory[dir_index]->name;
}

long dir_get_size(int dir_index)
{
    return directory[dir_index]->size;
}

void dir_inc_size(int dir_index, long delta)
{
    directory[dir_index]->size += delta;
}

void dir_remove(int dir_index)
{
    free(directory[dir_index]);
    directory[dir_index] = NULL;
}