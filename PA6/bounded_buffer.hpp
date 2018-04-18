//
//  bounded_buffer.hpp
//  
//
//  Created by Joshua Higginbotham on 11/4/15.
//
//

#ifndef bounded_buffer_h
#define bounded_buffer_h

#include <stdio.h>
#include <queue>
#include <string>
#include <pthread.h>
#include "Semaphore.hpp"

class bounded_buffer {
    /* Internal data here */
    unsigned long long size_;
    Semaphore mutex_, full_slots_, empty_slots_;
    std::queue<std::string> data_;
public:
    bounded_buffer(unsigned long long _capacity);
    ~bounded_buffer();

    void push_back(const std::string& str);
    std::string retrieve_front();
    unsigned long long size();
};

#endif /* bounded_buffer_h */
