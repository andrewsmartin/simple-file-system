#include "sblock_cache.h"
#include "sfs_types.h"
#include "sfs_constants.h"

#include "lib/disk_emu.h"

#include <string.h>

static struct
{
    const uint16_t block_size;
    const uint16_t num_blocks_root;
    uint16_t num_blocks_fat;
    const uint32_t num_data_blocks;
    uint32_t num_free_blocks;
} super_block = {.block_size = BLOCK_SIZE, .num_blocks_root = DIRECTORY_BLOCKS, 
    .num_data_blocks = TOTAL_DATA_BLOCKS};

void sbc_init()
{
    super_block.num_blocks_fat = FAT_BLOCKS;
	super_block.num_free_blocks = TOTAL_DATA_BLOCKS;
}

void sbc_load()
{
	byte buf[BLOCK_SIZE] = {0};
    read_blocks(0, 1, buf);
    memcpy(&super_block, buf, BLOCK_SIZE);
}

void sbc_flush()
{
    write_blocks(0, 1, &super_block);
}

void sbc_set_nfree(uint32_t n)
{
	super_block.num_free_blocks = n;
}