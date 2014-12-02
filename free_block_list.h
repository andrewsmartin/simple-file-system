#ifndef __FREE_BLOCK_LIST_H
#define __FREE_BLOCK_LIST_H

#include <stdint.h>

#include "sfs_types.h"
#include "bit_field.h"

typedef struct _FreeBlockList FreeBlockList;

void fbl_init();

void fbl_load();

void fbl_flush();

int fbl_get_free_index();

void fbl_set_free_index(uint32_t index);

uint32_t fbl_get_num_free();

byte *fbl_get_raw();

void fbl_set_raw(byte *bytes);

void fbl_destroy();

#endif