/* 
    File: my_allocator.h

    Author: R. Bettati and C. Johnson
            Department of Computer Science
            Texas A&M University
    Date  : 08/02/08

    Modified: 2018-02-02

*/

#ifndef _my_allocator_h_                   // include file only once
#define _my_allocator_h_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdint.h>

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

typedef void * Addr;

// These values are put in errno when something specific goes wrong.
enum my_errors {
    ERR_INVALID_STATE = -1   // called free when not initialized, or vice versa
};

// The struct used to track all of the parameters and moving parts of
// the allocator.
struct Allocator_struct {
    unsigned long int block_size;
    unsigned long int buf_size;
    unsigned long int total_blocks;
    void *buf = NULL;

    // bitmask of "free" status by block tree index
    uint8_t *free_bitmap = NULL;

    // Array of levels that each block's start belongs to.
    // Supports up to 64 levels due to lshift operations.
    // "Level" is x where a segment is 2^x blocks wide.
    int8_t *level_index = NULL;
};


/*--------------------------------------------------------------------------*/
/* FORWARDS */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* MODULE   MY_ALLOCATOR */
/*--------------------------------------------------------------------------*/

unsigned long int init_allocator(unsigned long int _basic_block_size, 
                                 unsigned long int _length); 
/* This function initializes the memory allocator and makes a portion of 
   ’_length’ bytes available. The allocator uses a ’_basic_block_size’ as 
   its minimal unit of allocation. The function returns the amount of 
   memory made available to the allocator. If an error occurred, 
   it returns 0. 
*/

int release_allocator(); 
/* This function returns any allocated memory to the operating system. 
   After this function is called, any allocation fails.
*/

void release_allocator_noreturn();
// Calls release_allocator() without returning, for atexit use

Addr my_malloc(unsigned int _length);
Addr my_malloc(unsigned long int _length);
/* Allocate _length number of bytes of free memory and returns the 
   address of the allocated portion. Returns 0 when out of memory. */ 

int my_free(Addr _a); 
/* Frees the section of physical memory previously allocated 
   using ’my_malloc’. Returns 0 if everything ok. */ 

// Not implemented, see print_tree() and print_levels()
//void print_allocator();
/* Mainly used for debugging purposes and running short test cases */
/* This function should print how many free blocks of each size belong to the allocator
at that point. The output format should be the following (assuming basic block size = 128 bytes):

128: 5
256: 0
512: 3
1024: 0
....
....
 which means that at point, the allocator has 5 128 byte blocks, 3 512 byte blocks and so on.*/

void print_tree();
void print_levels();
/* These two functions output the current state of the allocator's free
 * bitmask and level array respectively. When used together, it looks like:
 * 1
 * 1                               1
 * 1               0               0               0
 * 0       0       0       0       0       0       0       0
 * 0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0
 * 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
 * 3 F F F F F F F F F F F F F F F 4 F F F F F F F F F F F F F F F
 */
#endif 
