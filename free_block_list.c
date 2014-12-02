#include "free_block_list.h"
#include "bit_field.h"
#include "sfs_types.h"
#include "sfs_constants.h"

#include "lib/disk_emu.h"

#include <stdlib.h>
#include <string.h>

static BitField *bfield;

void fbl_init()
{
    bfield = bf_create(TOTAL_DATA_BLOCKS);
    bf_set_all_bits(bfield, 1);
}

void fbl_load()
{
    byte buf[BLOCK_SIZE] = {0};
    read_blocks(FREE_LIST_START, 1, buf);
}

void fbl_flush()
{
    write_blocks(FREE_LIST_START, 1, bf_get_raw_bytes(bfield));
}

int fbl_get_free_index()
{
    uint32_t ret = bf_locate_first(bfield, 1);
    bf_flip_bit(bfield, ret);
    return ret;
}

void fbl_set_free_index(uint32_t index) {
    bf_flip_bit(bfield, index);
}

uint32_t fbl_get_num_free()
{
    return bf_num_one_bits(bfield);
}

byte *fbl_get_raw()
{
    return bf_get_raw_bytes(bfield);
}

void fbl_set_raw(byte *bytes)
{
    bf_set_raw_bytes(bfield, bytes);
}

void fbl_destroy()
{
    bf_destroy(bfield);
}