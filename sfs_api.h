#ifndef __SFS_API_H
#define __SFS_API_H

/* Creates the file system. */
void mksfs(int fresh);

/* Lists files in the root directory. */
void sfs_ls();

/* Opens the given file and returns a file descriptor id. */
int sfs_fopen(char *name);

/* Closes the given file. */
void sfs_fclose(int fileID);

/* Writes [buf] characters onto the disk. */
void sfs_fwrite(int fileID, char *buf, int length);

/* Reads characters from disk into [buf]. */
void sfs_fread(int fileID, char *buf, int length);

/* Seek to [loc] bytes from the beginning. */
void sfs_fseek(int fileID, int loc);

/* Removes the given file from the file system. */
int sfs_remove(char *file);

#endif