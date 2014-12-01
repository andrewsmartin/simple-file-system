#include <stdlib.h>

#include "free_block_list.h"
#include "sfs_types.h"
#include "bit_field.h"

struct _FreeBlockList
{
    BitField *bfield;
};

FreeBlockList *fbl_create(uint32_t num_blocks)
{
    FreeBlockList *ret = malloc(sizeof(FreeBlockList));
    ret->bfield = bf_create(num_blocks);
    bf_set_all_bits(ret->bfield, 1);
    return ret;
}

int fbl_get_free_index(FreeBlockList *fblist)
{
    uint32_t ret = bf_locate_first(fblist->bfield, 1);
    bf_flip_bit(fblist->bfield, ret);
    return ret;
}

void fbl_set_free_index(FreeBlockList *fblist, uint32_t index) {
    bf_flip_bit(fblist->bfield, index);
}

uint32_t fbl_get_num_free(FreeBlockList *fblist)
{
    return bf_num_one_bits(fblist->bfield);
}

byte *fbl_get_raw(FreeBlockList *fblist)
{
    return bf_get_raw_bytes(fblist->bfield);
}

void fbl_set_raw(FreeBlockList *fblist, byte *bytes)
{
    bf_set_raw_bytes(fblist->bfield, bytes);
}

void fbl_destroy(FreeBlockList *fblist)
{
    bf_destroy(fblist->bfield);
    free(fblist);
    fblist = NULL;
}