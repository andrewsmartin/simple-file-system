#ifndef __FAT_CACHE_H
#define __FAT_CACHE_H

#include "sfs_errors.h"

/* Initialize the cache. */
void fat_init();

/* Load the on-disk FAT into memory. */
void fat_load();

/* Writes the contents of the cached FAT to disk. */
void fat_flush();

/* Creates a new entry in the FAT without allocating
   a data block. Returns the index in the table if
   successful, or ERR_OUT_OF_SPACE if there is no
   more room in the table. */
int fat_create_entry();

/* Traverses the chain of FAT entries starting at fat_index,
   returning the final index in the chain. */
int fat_get_tail(int fat_index);

/* Returns the data block index pointed to by the given fat entry. */
int fat_get_data_block(int fat_index);

/* Returns the next fat_index. */
int fat_get_next_index(int fat_index);

/* Set the next fat index in the chain. */
void fat_set_next_index(int fat_index, int next);

int fat_alloc_block(int fat_index);

void fat_clean_entry(int fat_root);

#endif