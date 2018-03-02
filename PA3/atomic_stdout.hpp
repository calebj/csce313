#ifndef ATOMIC_STDOUT_HPP
#define ATOMIC_STDOUT_HPP

#include <pthread.h>
#include <cstring>
#include <iostream>
/*
    The class below allows any thread in this program to use
    the variable "atomic_standard_output" down below to write output to the
    console without it getting jumbled together with the output
    from other threads.
    For example:
    thread A {
        atomic_standard_output.println("Hello ");
    }
    thread B {
        atomic_standard_output.println("World!\n");
    }
    The output could be:

    Hello
    World!

    or...

    World!

    Hello

    ...but it will NOT be something like:

    HelWorllo
    d!

 */

class atomic_standard_output {
    /*
         Note that this class provides an example
         of the usage of mutexes, which are a crucial
         synchronization type. You will probably not
         be able to complete this assignment without
        using mutexes.
     */
    pthread_mutex_t console_lock;
public:
    atomic_standard_output() {
        pthread_mutex_init(&console_lock, nullptr);
    }
    ~atomic_standard_output() {
        pthread_mutex_destroy(&console_lock);
    }
    void println(std::string s) {
        pthread_mutex_lock(&console_lock);
        std::cout << s << std::endl;
        pthread_mutex_unlock(&console_lock);
    }
    void perror(std::string s) {
        pthread_mutex_lock(&console_lock);
        std::cerr << s << ": " << strerror(errno) << std::endl;
        pthread_mutex_unlock(&console_lock);
    }
};

#endif //ATOMIC_STDOUT_HPP
