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

class semaphore_exception : public std::exception {
    std::string err = "failure in sync library";

public:
    semaphore_exception() {}
    semaphore_exception(std::string msg) : err(msg) {}

    virtual const char* what() const throw() {
        return err.c_str();
    }
};


class SemaphoreBase {

public:

    /* -- CONSTRUCTOR/DESTRUCTOR */

    SemaphoreBase(const unsigned long long& _val = 0) {};
    virtual ~SemaphoreBase() {};
    /* Clearer names for the functions */

    virtual void notify() = 0;
    virtual void wait() = 0;
    virtual void destroy() {};

    /* -- SEMAPHORE OPERATIONS */

    inline void P() { wait(); }

    inline void V() { notify(); }
};

class Semaphore : public SemaphoreBase {
private:
    /* -- INTERNAL DATA STRUCTURES */
    unsigned long long val_;
    std::mutex mutex_;
    std::condition_variable condition_;
public:

    Semaphore(const unsigned long long& val = 0) : val_(val) {}

    void notify() override {
        // RAII handle on mutex
        std::unique_lock<std::mutex> lock(mutex_);
        // Increment value
        ++val_;
        // Allow one waiting thread to proceed
        condition_.notify_one();
    }

    void wait() override {
        // RAII handle on mutex
        std::unique_lock<std::mutex> lock(mutex_);
        // Guard against dropping below 0
        while (!val_) condition_.wait(lock);
        // Decrement value
        --val_;
    }
};

#endif


