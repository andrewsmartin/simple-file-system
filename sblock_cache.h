#ifndef __SBLOCK_CACHE_H
#define __SBLOCK_CACHE_H

#include <stdint.h>

/* Initialize the cache. */
void sbc_init();

/* Load the on-disk super block into memory. */
void sbc_load();

/* Writes the contents of the cached super block to disk. */
void sbc_flush();

/* Set the free block count. */
void sbc_set_nfree(uint32_t n);

#endif