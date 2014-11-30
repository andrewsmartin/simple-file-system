#ifndef __BIT_FIELD_H
#define __BIT_FIELD_H

#include <stdint.h>

#include "sfs_types.h"

typedef struct _BitField BitField;

BitField *bf_create(uint32_t num_bits);

int bf_set_all_bits(BitField *b_field, int val);

byte *bf_get_raw_bytes(BitField *b_field);

void bf_set_raw_bytes(BitField *b_field, byte *bytes);

uint32_t bf_locate_first(BitField *b_field, int val);

uint32_t bf_num_one_bits(BitField *b_field);

int bf_set_bit(BitField *b_field, uint32_t index, int val);

void bf_destroy(BitField *b_field);

void bf_print_hex(BitField *b_field);

#endif