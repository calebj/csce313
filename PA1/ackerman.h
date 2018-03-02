/* 
    File: ackerman_auto.h

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 08/02/08
    Modified: 2018-01-27 by Caleb Johnson for non-interactive operation.

    Header file for the ackerman(n,m) function.
    This function is to be called as part of the "memtest" program
    in MP1 for CPSC 313.
*/

#ifndef _ackerman_auto_h_                              /* include file only once */
#define _ackerman_auto_h_

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

struct Ackerman_Results {
    int n, m;
    unsigned long int result;
    unsigned long long usec;
    unsigned long int num_allocations;
};

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */


/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* MODULE ackerman_auto */
/*--------------------------------------------------------------------------*/

extern Ackerman_Results ackerman_auto(unsigned int n, unsigned int m);
/* Using the provided parameters n and m, computes the result of the
   (highly recursive!) ackerman function. During every recursion step,
   it allocates and de-allocates a portion of memory with the use of the
   memory allocator defined in module "my_allocator.H".
*/

extern void print_ackerman_results(Ackerman_Results results);

#endif
