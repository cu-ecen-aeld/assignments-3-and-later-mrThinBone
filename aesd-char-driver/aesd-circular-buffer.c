/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include <stdio.h>
#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    // printf(">>> LOOKING FOR: %ld\n", char_offset);
    uint8_t entry_offset = buffer->out_offs;
    size_t offset = 0;
    int i = 1;
    do {
        // printf(">>> current offset: %ld, next buff[%d]: %ld <<<\n", offset, entry_offset, buffer->entry[entry_offset].size);
        if (offset + buffer->entry[entry_offset].size > char_offset) {
            *entry_offset_byte_rtn = char_offset - offset;
            return &buffer->entry[entry_offset];
        }
        offset += buffer->entry[entry_offset].size;
        entry_offset++;
        if (entry_offset >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
            entry_offset = 0;
        }
    } while (++i <= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    bool full = buffer->full;
    uint8_t in_offset = buffer->in_offs;

    if (full) {
        // free(buffer->entry[in_offset].buffptr);
        buffer->entry[in_offset].buffptr = NULL;
        buffer->entry[in_offset].size = 0;
    }

    buffer->entry[in_offset] = *add_entry;

    bool first_reach = false;

    if (in_offset + 1 >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        buffer->in_offs = 0;
        if (!full) {
            first_reach = 1;
        }
    } else {
        buffer->in_offs++;
    }
    
    if (first_reach) {
        buffer->full = true;
    } else {
        if (full) {
            buffer->out_offs++;
            if (buffer->out_offs >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
                buffer->out_offs = 0;
            }
        }
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
