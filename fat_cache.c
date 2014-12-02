#include "fat_cache.h"
#include "sfs_types.h"
#include "sfs_constants.h"
#include "free_block_list.h"

#include "lib/disk_emu.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    byte used;
    int data_block;
    int next;
} FatEntry;

/* Number of fat entries should be equal to number of data blocks. */
#define _FAT_BYTES (sizeof(FatEntry) * TOTAL_DATA_BLOCKS)
#define _FAT_BLOCKS (_FAT_BYTES / BLOCK_SIZE)

const int FAT_BYTES = _FAT_BYTES;
const int FAT_BLOCKS = _FAT_BLOCKS;
const int DATA_BLOCK_OFFSET = FAT_START + _FAT_BLOCKS + FREE_LIST_LEN;

#undef _FAT_BYTES
#undef _FAT_BLOCKS

/* There can be at most as many FAT entries as there are data blocks. */
static FatEntry *fat_table[TOTAL_DATA_BLOCKS];

void fat_load()
{
    const int len = sizeof(FatEntry);
	int i;
    byte *buf = malloc(FAT_BYTES), *ptr = buf;
    read_blocks(FAT_START, FAT_BLOCKS, buf);
    for (i = 0; i < FAT_BYTES; i += len, ptr += len)
    {
        // First byte tells us whether or not this slot is used.
        if (buf[i] == 1) {
            FatEntry *fat_entry = malloc(len);
            memcpy(fat_entry, ptr, len);
            fat_table[i / len] = fat_entry;
        }
    }
    free(buf);
}

void fat_flush()
{
	byte *buf = calloc(FAT_BYTES, 1), *ptr = buf;
    int i;
    for (i = 0; i < TOTAL_DATA_BLOCKS; i++, ptr += sizeof(FatEntry)) {
        FatEntry *f = fat_table[i];
        if (f != NULL)
            memcpy(ptr, f, sizeof(FatEntry));
    }

    write_blocks(FAT_START, FAT_BLOCKS, buf);
    free(buf);
}

int fat_create_entry()
{
    int i;
    for (i = 0; i < TOTAL_DATA_BLOCKS; i++) {
        if (fat_table[i] == NULL) break;
    }

    if (i == TOTAL_DATA_BLOCKS) return ERR_OUT_OF_SPACE;

    FatEntry *fat = malloc(sizeof(FatEntry));
    fat->used = 1;
    fat->data_block = NO_DATA;
    fat->next = END_OF_FILE;
    fat_table[i] = fat;

    return i;
}

int fat_get_tail(int fat_index)
{
    while (fat_table[fat_index]->next != END_OF_FILE) 
        fat_index = fat_table[fat_index]->next;
    return fat_index;
}

int fat_get_data_block(int fat_index)
{
    return fat_table[fat_index]->data_block;
}

int fat_get_next_index(int fat_index)
{
    return fat_table[fat_index]->next;
}

void fat_set_next_index(int fat_index, int next)
{
    fat_table[fat_index]->next = next;
}

int fat_alloc_block(int fat_index)
{
    // Find a free data block from the free list.
    int db = fbl_get_free_index();

    if (db < 0) {
        return ERR_OUT_OF_SPACE;
    }

    fat_table[fat_index]->data_block = db + DATA_BLOCK_OFFSET;

    return 0;
}

void fat_clean_entry(int fat_root)
{
    int fat_index = fat_root;
    while (fat_index != END_OF_FILE) {
        FatEntry *f = fat_table[fat_index];
        fbl_set_free_index(f->data_block);
        fat_table[fat_index] = NULL;
        fat_index = f->next;
        free(f);
    }
}