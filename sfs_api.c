#include "sfs_api.h"
#include "sfs_constants.h"
#include "sblock_cache.h"
#include "dir_cache.h"
#include "fat_cache.h"
#include "free_block_list.h"
#include "file_descriptor.h"

#include "lib/disk_emu.h"

#include <stdio.h>

#define DISK_FILE "test.disk"

static void flush_caches();
static void load_all_caches();
static int create_file(char *name);
static void init_caches();

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

    printf("\nListing files...\n");
    printf("%-30s%-20s%-10s\n", "Name", "Size (bytes)", "FAT Index");

    dir_iter_begin();
    while (!dir_iter_done()) {
        printf("%-30s%-20ld%-10d\n", 
            dir_get_name(dir_curr_iter()), 
            dir_get_size(dir_curr_iter()),
            dir_get_fat_root(dir_curr_iter()));
        dir_iter_next();
    }

    printf("\n");
}

int sfs_fopen(char *name)
{
    int fileID = fdesc_search(name);
    if (fileID == ERR_NOT_FOUND) {
        int dir_index = dir_search(name);
        if (dir_index == ERR_NOT_FOUND) {
            dir_index = create_file(name);
            if (dir_index == ERR_OUT_OF_SPACE) {
                puts("No space to create the file.");
                return ERR_OUT_OF_SPACE;
            }
        }
        fileID = fdesc_create(dir_index);
    }

    return fileID;
}

void sfs_fclose(int fileID)
{
    int result = fdesc_remove(fileID);
    if (result == ERR_NOT_FOUND) {
        printf("No file open with id %d\n,  not closing.", fileID);
        return;
    }
}

void sfs_fwrite(int fileID, char *buf, int length)
{
    fdesc_write(fileID, buf, length);
    flush_caches();
}

void sfs_fread(int fileID, char *buf, int length)
{
    fdesc_read(fileID, buf, length);
}

void sfs_fseek(int fileID, int loc)
{
    fdesc_seek(fileID, loc);
}

int sfs_remove(char *file)
{   
    int fileID = fdesc_search(file);
    if (fileID != ERR_NOT_FOUND)
        sfs_fclose(fileID);

    int dir_index = dir_search(file);
    if (dir_index == ERR_NOT_FOUND) {
        printf("No file exists with name %s\n.", file);
        return -1;
    }

    // Free up the fat entries and associated data blocks.
    fat_clean_entry(dir_get_fat_root(dir_index));
    dir_remove(dir_index);
    flush_caches();

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
    int dir_index = dir_create_entry(name);
    if (dir_index == ERR_OUT_OF_SPACE) return ERR_OUT_OF_SPACE;
    
    flush_caches();
    return dir_index;
}

void flush_caches()
{
    sbc_set_nfree(fbl_get_num_free());
    sbc_flush();
    dir_flush();
    fat_flush();
    fbl_flush();
}

void load_all_caches()
{
    sbc_load();
    dir_load();
    fat_load();
    fbl_load();
}

void init_caches()
{
    sbc_init();
    fbl_init();
}