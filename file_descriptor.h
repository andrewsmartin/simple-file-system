#ifndef __FILE_DESCRIPTOR_H
#define __FILE_DESCRIPTOR_H

#include "sfs_errors.h"

/* Searches the file descriptor table for an open file with
   the given name. Returns the file descriptor ID if found,
   or ERR_NOT_FOUND otherwise. */
int fdesc_search(char *name);

/* Creates a new file descriptor entry in the table and returns
   the fileID if sucessful. Returns ERR_MAX_OPEN otherwise. */
int fdesc_create(int dir_index);

/* Removes the file descriptor pointed to by fileID. Returns 0
   on a successful remove, or ERR_NOT_FOUND otherwise. */
int fdesc_remove(int fileID);

int fdesc_write(int fileID, char *buf, int length);

int fdesc_read(int fileID, char *buf, int length);

int fdesc_seek(int fileID, int loc);

#endif