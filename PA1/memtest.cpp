/* 
 *  File: memtest.cpp
 * 
 *  Author: Caleb Johnson
 *          Department of Computer Science
 *          Texas A&M University
 *  Date  : 2018-01-27
 * 
 *  This file contains the main function for PA1. The ackerman function
 *  is used to test the buddy allocator implemented for the assignment.
 * 
 */

#include "ackerman.h"
#include "my_allocator.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

static unsigned long int DEFAULT_BLOCKSIZE = 128,
                         DEFAULT_MEMSIZE = 524288; // 512 kB
static unsigned int DEFAULT_ACKERMAN_N = 3, // default for auto mode
                    DEFAULT_ACKERMAN_M = 6; // ditto

void print_usage(char **argv) {
    printf("Usage:\n %s [-h] [-a] [-b <blocksize>] [-s <memsize>] [-n <n>] [-m <m>]\n", argv[0]);
    printf("\nOptions:\n");
    printf(" -h             Prints this help and exits.\n");
    printf(" -b <blocksize> basic block size, in bytes. Defaults to %lu.\n", DEFAULT_BLOCKSIZE);
    printf(" -s <memsize>   size of memory to be allocated, in bytes. Defaults to %lu.\n", DEFAULT_MEMSIZE);
    printf(" -n <n>         N parameter for non-interactive ackerman computation. Defaults to %i.\n", DEFAULT_ACKERMAN_N);
    printf(" -m <m>         M parameter for non-interactive ackerman computation. Defaults to %i.\n", DEFAULT_ACKERMAN_M);
}

unsigned long int check_ul_param(char *arg) {
    char *endptr;
    unsigned long int tmp;

    tmp = strtoul(arg, &endptr, 10);

    if ((errno == ERANGE && (tmp == LONG_MAX || tmp == LONG_MIN)) || (errno != 0 && tmp == 0)) {
        perror("memsize");
        exit(EXIT_FAILURE);
    }

    return tmp;
}


int main(int argc, char **argv) {
    // Initialize operating parameters to defaults
    unsigned long int basic_block_size = DEFAULT_BLOCKSIZE,
                      memory_length = DEFAULT_MEMSIZE;

    unsigned int ackerman_n = DEFAULT_ACKERMAN_N,
                 ackerman_m = DEFAULT_ACKERMAN_M;

    // Used for argument parsing
    int index, c;
    opterr = 0;
    errno = 0;

    // Argument parsing loop
    while ((c = getopt(argc, argv, "hb:s:n:m:")) != -1)
        switch (c) {
            case 'h':
                print_usage(argv);
                exit(EXIT_SUCCESS);
            case 'b':
                basic_block_size = check_ul_param(optarg);
                break;
            case 's':
                memory_length = check_ul_param(optarg);
                break;
            case 'm':
                ackerman_m = check_ul_param(optarg);
                break;
            case 'n':
                ackerman_n = check_ul_param(optarg);
                break;
            case '?':
                if (optopt == 'b' || optopt == 's')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                exit(EXIT_FAILURE);
            default:
                abort();
        }

    for (int index = optind; index < argc; index++)
        fprintf(stderr, "WARN: Ignoring non-option argument %s\n", argv[index]);

    if(init_allocator(basic_block_size, memory_length) == 0) {
        fprintf(stderr, "FATAL: Failed to initialize allocator!");
        exit(EXIT_FAILURE);
    }

    // Free allocator memory when interrupted
    atexit(release_allocator_noreturn);

    // Print message to show we started
    printf("Begin computation of ackerman(%d, %d)...\n", ackerman_n, ackerman_m);

    // Chain computation and print calls for neatness
    print_ackerman_results(ackerman_auto(ackerman_n, ackerman_m));

    // Free memory used by allocator before exiting
    release_allocator();
}
