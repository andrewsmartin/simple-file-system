#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "bit_field.h"
#include "sfs_types.h"

struct _BitField
{
    uint32_t num_bytes;
    byte* bits;
};

static byte int2byte(int val);

BitField *bf_create(uint32_t num_bits)
{
    BitField *ret = malloc(sizeof(BitField));
    ret->num_bytes = num_bits / 8;
    ret->bits = calloc(num_bits, 1);
    return ret;
}

int bf_set_all_bits(BitField *b_field, int val)
{
    /*byte byte_val;
    if (val == 0) 
        byte_val = 0;
    else if (val == 1)
        byte_val = 255;
    else
        return -1;*/

    byte byte_val = int2byte(val);
    int i;
    for (i = 0; i < b_field->num_bytes; i++) {
        b_field->bits[i] = byte_val;
    }

    return 0;
}

byte *bf_get_raw_bytes(BitField *b_field)
{
    return b_field->bits;
}

void bf_set_raw_bytes(BitField *b_field, byte *bytes)
{
    memcpy(b_field->bits, bytes, b_field->num_bytes);
}

uint32_t bf_locate_first(BitField *b_field, int val)
{
    /*byte byte_val;
    if (val == 0) 
        byte_val = 0;
    else if (val == 1)
        byte_val = 255;
    else
        return -1;*/
    byte byte_val = int2byte(val);

    int i;
    for (i = 0; i < b_field->num_bytes; i++) {
        int j;
        byte curr_byte = b_field->bits[i];
        for (j = 7; j >= 0; j--) {
            byte chk = (curr_byte >> j) & 1;
            if (chk == byte_val)
                return i * 8 + (7 - j);
        }
    }

    return -1;
}

uint32_t bf_num_zero_bits(BitField *b_field)
{
    uint32_t count = 0, i;
    for (i = 0; i < b_field->num_bytes; i++) {
        byte curr_byte = b_field->bits[i];
        int j;
        for (j = 0; j < 8; j++) {
            curr_byte = curr_byte >> 1;
            byte chk = curr_byte & 1;
            if (chk == 1)
                count++;
        }
    }

    return count;
}

int bf_set_bit(BitField *b_field, uint32_t index, int val)
{
    if (index >= b_field->num_bytes * 8)
        return -1;
    byte byte_val = int2byte(val);
    b_field->bits[index] = byte_val;
    return 0;
}

void bf_destroy(BitField *b_field)
{
    free(b_field->bits);
    free(b_field);
    b_field = NULL;
}

void bf_print_hex(BitField *b_field)
{
    int i;
    char buf[5];
    for (i = 0; i < b_field->num_bytes; i += 2) {
        sprintf(buf, "%02x", b_field->bits[i]);
        sprintf(buf + 2, "%02x", b_field->bits[i + 1]);
        printf("%s%c", buf, ((i + 2) % 16 == 0) ? '\n' : ' ');
    }
}

/*** Private helper methods. ***/
byte int2byte(int val)
{
    return val >= 255 ? 255 : (byte)val;
}