/* 
    File: my_allocator.cpp

    Author: Caleb Johnson
            Department of Computer Science
            Texas A&M University
    Date  : 2018-01-27

    This file contains the implementation of the module "MY_ALLOCATOR".

*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include "my_allocator.h"

// Global variable for allocator
Allocator_struct *allocator = NULL;

/*--------------------------------------------------------------------------*/
/* CONVENIENCE FUNCTIONS FOR MODULE MY_ALLOCATOR                            */
/*--------------------------------------------------------------------------*/

// Counts the 1s in a binary number
// from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan
inline unsigned int _ones(register unsigned long int v) {
    unsigned int c; // c accumulates the total bits set in v
    for (c = 0; v; c++) {
        v &= v - 1; // clear the least significant bit set
    }
    return c;
}

// Calculates the next power of two of a given number.
// from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
inline unsigned int _next_power_of_two(register unsigned long int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

// Calculates the FLOOR log2 of an integer
// from http://aggregate.org/MAGIC/#Log2%20of%20an%20Integer
inline unsigned int floor_log2(register unsigned long int x) {
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    #ifdef LOG0UNDEFINED
        return(_ones(x) - 1);
    #else
        return(_ones(x >> 1));
    #endif
}

// Calculates the CEILING log2 of an integer
// from http://aggregate.org/MAGIC/#Log2%20of%20an%20Integer
inline unsigned int log2(register unsigned long int x) {
    register int y = (x & (x - 1));

    y |= -y;
    y >>= (WORD_BIT - 1);
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    #ifdef LOG0UNDEFINED
        return(_ones(x) - 1 - y);
    #else
        return(_ones(x >> 1) - y);
    #endif
}

// int-array bitfield stuff, reversed but it doesn't matter
inline bool _get_bit(register uint8_t *bitmask, register unsigned long int index) {
    return (bitmask[index >> 3] & (0b10000000 >> (index % 8)));
}

inline void _set_bit(uint8_t *bitmask, unsigned long int index) {
    register uint8_t overlay = (0b10000000 >> (index % 8));
    bitmask[index >> 3] |= overlay;
}

// return true if bit is already set, otherwise false
inline bool _ensure_bit(uint8_t *bitmask, unsigned long int index) {
    register uint8_t overlay = (0b10000000 >> (index % 8));
    if(bitmask[index >> 3] & overlay) return false;
    bitmask[index >> 3] |= overlay;
    return true;
}

inline bool _flip_bit(uint8_t *bitmask, unsigned long int index) {
    return (bitmask[index >> 3] ^= (0b10000000 >> (index % 8))) > 0;
}

inline void _clear_bit(uint8_t *bitmask, unsigned long int index) {
    bitmask[index >> 3] &= (0b11111111 - (0b10000000 >> (index % 8)));
}

// Prints out a tree representation of the free bitmap
// Example:
// 1
// 1                               1
// 1               0               0               0
// 0       0       0       0       0       0       0       0
// 0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
extern void print_tree() {
    uint8_t *bitmask = allocator->free_bitmap;
    unsigned long int length = (allocator->buf_size/allocator->block_size)*2;
    int8_t tier = 0,
           max_tier = log2(length),
           step = 1 << (max_tier - 1);
    printf(" ");

    for(unsigned long int i = 0; i < length ;) {
        printf(_get_bit(bitmask, i) ? "1" : "0");

        if((++i + 1) % (1 << tier)) {
            for(int j=0; j<(step-1); j++) printf(" ");
        } else {
            if(++tier == max_tier) break;
            step = 1 << (max_tier - tier);
            printf("\n ");
        }
    }
    printf("\n");
}

// Prints out a single line of two-character wide levels.
// The left character is padded with a space unless the number is
// 10 or more. 'F' is used to represent a free space.
extern void print_levels() {
    int8_t level;
    for(unsigned long int i=0; i < allocator->total_blocks; i++) {
        level = allocator->level_index[i];
        if(level >= 0)
            printf("% 2i", level);
        else
            printf(" F");
    }
    printf("\n");
}

inline unsigned long int _bitmask_index_to_block(unsigned long int i) {
    // log2(index + 2) is this index's "rank" from largest to smallest
    // 0 is 1, 1 and 2 are 2, 3-6 are 3, 7-14 are 4, etc.
    // Divide the total by that to get the number of blocks per chunk.
    // Block num is *which* of those chunks the index corresponds to.
    // For example, 8 is the 2nd of the quarter-wide blocks.
    // It works because subtracting 1 << (level_chunks - 1) "normalizes"
    // the count.

    unsigned long int index_rank = log2(i+2),
        chunk_width = allocator->total_blocks / (1 << (index_rank - 1)),
        chunk_num = i + 1 - (1 << (index_rank - 1));

    return chunk_width * chunk_num;
}

inline unsigned long int _block_to_bitmask_index(unsigned long int block, int8_t level) {
    // Not only does this function give the index of blocks of a certain level, but it also
    // "rounds down" such that no more code is needed to walk to the "root" block. One
    // simply needs to raise the level until the returned value is zero!

    unsigned long int width = 1 << level, // Blocks per chunk at this level.
        which = (block / width) - 1,      // Zero-based index within level.
        chunks = allocator->total_blocks/width; // Bitmask index starts from how many
                                                // chunks are in each level. Convenient!
    return chunks + which;
}

long int _next_free_block(unsigned int level, unsigned long int position = 0) {
    // Finds the index of the next free block of a desired level (2^level blocks wide)
    // Optionally takes a starting index for the search (bounds not checked!)

    // Highest free-mask index this block can fit in.
    // Example: full-width is 0, half is 2, quarter is 6, 14, 30, etc.
    unsigned long int mask_upper = (allocator->total_blocks / (1 << level) - 1) << 1,
                      // Lowest freemask index the block can fit in.
                      // Larger blocks aren't a part of the equation because
                      // all possible options are part of the range here.
                      mask_lower = _block_to_bitmask_index(0, level);

    int8_t cursor_level;

    for(unsigned long int i = mask_lower; i <= mask_upper ; i++) {
        unsigned long int block = _bitmask_index_to_block(i);

        cursor_level = allocator->level_index[block];

        if(cursor_level >= 0) {
            // "skip" the next n bits if the current one is wider than one
            if (cursor_level > level)
                i += (1 << (cursor_level - level)) - 1;
        } else if(!_get_bit(allocator->free_bitmap, i)) {
            return _bitmask_index_to_block(i);
        }

    }

    // There's no free space for another block :(
    return -1;
}

// Initialize allocator, allocate underlying buffers, and clear memory
extern unsigned long int init_allocator(unsigned long int block_size,
                                        unsigned long int mem_size) {

    if(allocator != NULL) {
        errno = ERR_INVALID_STATE;
        return 0;
    }

    // test for powers of 2 and promote it if it isn't one
    if(block_size & (block_size - 1)) {
        block_size = _next_power_of_two(block_size);
    }

    if(mem_size & (mem_size - 1)) {
        mem_size = _next_power_of_two(mem_size);
    }

    // Populate allocator struct parameters
    allocator = (Allocator_struct*)malloc(sizeof(Allocator_struct));
    allocator->buf_size = mem_size;
    allocator->block_size = block_size;
    allocator->total_blocks = mem_size / block_size;

    // Allocate underlying memory buffer
    allocator->buf = malloc(mem_size);
    if(allocator->buf == NULL) return 0;
    memset(allocator->buf, 0, mem_size);

    // Compute size, allocate and zero freemap
    // unsigned int levels = log2(mem_size/block_size);     // maximum #levels in buddy "tree"
    // unsigned long int freemap_bytes = (1 << (levels-2)); // div/8 is same as rshift by 3
    unsigned long int freemap_bytes = (mem_size/block_size)/4;
    allocator->free_bitmap = (uint8_t*)malloc(freemap_bytes);
    if(allocator->free_bitmap == NULL) return 0;
    memset(allocator->free_bitmap, 0, freemap_bytes);

    // Allocate and zero level index
    allocator->level_index = (int8_t*)malloc(mem_size/block_size);
    if(allocator->level_index == NULL) return 0;
    memset(allocator->level_index, -1, mem_size/block_size);

    return mem_size;
}

extern int release_allocator() {
    if(allocator == NULL) {
        errno = ERR_INVALID_STATE;
        return 1;
    }

    // Free memory allocated to struct members, then the struct itself
    free(allocator->buf);
    free(allocator->free_bitmap);
    free(allocator->level_index);
    free(allocator);
    allocator = NULL;
    return 0;
}

// Simple wrapper to keep compiler from from complaining about type passed to atexit()
extern void release_allocator_noreturn() {
    release_allocator();
}

void _mark_bitmask_allocated(unsigned long int block, int8_t level) {
    unsigned long int index = _block_to_bitmask_index(block, level);
    unsigned long int chunk = block/(1 << level),
                      chunks = allocator->total_blocks/(1 << level);

    // If already set, we can safely assume the parent blocks have also
    // been marked, and stop advancing up the tree here.
    if(_get_bit(allocator->free_bitmap, index)) {
        return;
    }

    _set_bit(allocator->free_bitmap, index);

    if(index == 0) return;
    _mark_bitmask_allocated(block, level + 1);
}

void _merge_buddies(unsigned long int block, int8_t level) {
    // If index is odd, we are left half. Even, right half. Helpful!
    unsigned long int index = _block_to_bitmask_index(block, level),
                      buddy = (index % 2) ? index + 1 : index - 1;

    _clear_bit(allocator->free_bitmap, index);

    if(index > 0 && !_get_bit(allocator->free_bitmap, buddy)) {

        unsigned long int buddy_index = _bitmask_index_to_block(buddy);
        int8_t buddy_level = allocator->level_index[buddy_index];

        // Recurse to higher level (parent block)
        _merge_buddies(block, level + 1);
    }
}

extern Addr my_malloc(unsigned int _length) {
    return my_malloc((unsigned long int)_length);
}

#ifndef USE_MALLOC
extern Addr my_malloc(unsigned long int _length) {
    // Level (log2 of #blocks) to fit requested length,
    // automatically rounded to next power of 2 because of ceiling
    int level;

    if(_length <= allocator->block_size)
        level = 0;
    else
        level = log2(_length) - log2(allocator->block_size);

    long int next_free = _next_free_block(level);

    if (next_free >= 0) {
        // Mark as used
        _mark_bitmask_allocated(next_free, level);
        allocator->level_index[next_free] = level;

        return (char*)allocator->buf + next_free*allocator->block_size;
    }
    return 0;
}

extern int my_free(Addr _a) {
    // Find offset of address within overall buffer and find its block index
    unsigned long int block = ((char*)_a - (char*)allocator->buf) / allocator->block_size;
    int8_t level = allocator->level_index[block];

    // Mark as freed in level index and bitmask
    allocator->level_index[block] = -1;

    // Recurse up buddy tree, checking and merging adjacent free blocks
    _merge_buddies(block, level);

    return 0;
}

#else

extern Addr my_malloc(unsigned long int _length) {
    return malloc((size_t)_length);
}

extern int my_free(Addr _a) {
    free(_a);
    return 0;
}
#endif
