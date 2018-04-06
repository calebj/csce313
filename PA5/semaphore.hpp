/* 
    File: semaphore.H

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 08/02/11

*/

#ifndef _semaphore_H_                   // include file only once
#define _semaphore_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include <pthread.h>
#include <mutex>
#include <condition_variable>

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CLASS   s e m a p h o r e  */
/*--------------------------------------------------------------------------*/

class semaphore {
private:
    /* -- INTERNAL DATA STRUCTURES */
    unsigned long long val_;
    std::mutex mutex_;
    std::condition_variable condition_;

public:

    /* -- CONSTRUCTOR/DESTRUCTOR */

    semaphore(const unsigned long long& _val) : val_(_val) {}

    /* Clearer names for the functions */

    void notify() {
        // RAII handle on mutex
        std::unique_lock<std::mutex> lock(mutex_);
        // Increment value
        ++val_;
        // Allow one waiting thread to proceed
        condition_.notify_one();
    }

    void wait() {
        // RAII handle on mutex
        std::unique_lock<std::mutex> lock(mutex_);
        // Guard against dropping below 0
        while (!val_) condition_.wait(lock);
        // Decrement value
        --val_;
    }

    unsigned long long get() {
        std::unique_lock<std::mutex> lock(mutex_);
        return val_;
    }

    /* -- SEMAPHORE OPERATIONS */

    inline void P() {
        wait();
    }

    inline void V() {
        notify();
    }
};

#endif


