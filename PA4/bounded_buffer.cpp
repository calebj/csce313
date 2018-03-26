//
//  bounded_buffer.cpp
//  
//
//  Created by Joshua Higginbotham on 11/4/15.
//
//

#include "bounded_buffer.hpp"
#include <string>
#include <queue>

bounded_buffer::bounded_buffer(unsigned long long capacity)
 : mutex_(1),
   full_slots_(0),
   empty_slots_(capacity) {}

bounded_buffer::~bounded_buffer() {
    // ???
}

void bounded_buffer::push_back(const std::string& req) {
    empty_slots_.wait();
    mutex_.wait();
    data_.push(req);
    mutex_.notify();
    full_slots_.notify();
}

std::string bounded_buffer::retrieve_front() {
    full_slots_.wait();
    mutex_.wait();
    std::string ret = data_.front();
    data_.pop();
    mutex_.notify();
    empty_slots_.notify();
    return ret;
}

unsigned long long bounded_buffer::size() {
    return full_slots_.get();
}
