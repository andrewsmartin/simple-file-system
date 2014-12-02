#ifndef __DIR_CACHE_H
#define __DIR_CACHE_H

#include "sfs_errors.h"

/* Initialize the cache. */
void dir_init();

/* Load the on-disk directory into memory. */
void dir_load();

/* Writes the contents of the cached directory to disk. */
void dir_flush();

/* Initializes an iterator for traversing the directory. */
void dir_iter_begin();

/* Returns true if there are no more elements left. */
int dir_iter_done();

/* Sets the iterator to the next element. */
void dir_iter_next();

int dir_curr_iter();

/* Searches the directory for a file with the given name. Returns a
   directory index if found, or else returns ERR_NOT_FOUND. */
int dir_search(char *name);

/* Creates a new file entry in the directory with the given name
   and index to the FAT root for the file. Returns the directory
   index if successful, or ERR_OUT_OF_SPACE if there is no space
   in the directory. */
int dir_create_entry(char *name);

/* Returns the root fat index for the file pointed to by dir_index. */
int dir_get_fat_root(int dir_index);

/* Returns the name of the file pointed to by dir_index. */
char *dir_get_name(int dir_index);

/* Returns the size of the file pointed to by dir_index in bytes. */
long dir_get_size(int dir_index);

/* Increment the size (in bytes) of the file pointed to by dir_index. */
void dir_inc_size(int dir_index, long delta);

void dir_remove(int dir_index);

#endif