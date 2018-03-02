//
//  RAIILock.h
//
//  Created by Caleb Johnson on 2018-03-01.
//
//  A simple mixin to aid in RAII-style locking of (almost) any class.
//
//  To use: inherit from RAIILockBase, and call:
//
//  RAIILock lock(*this);
//
//  within the critical scope. Once lock is deleted, another call can proceed.
//

#ifndef RAIILock_hpp
#define RAIILock_hpp

#include <mutex>

class RAIILockBase {
    friend class RAIILock;
private:
    std::mutex mutex_;
};

class RAIILock {
private:
    RAIILockBase& parent_;
public:
    RAIILock(RAIILockBase& parent) : parent_(parent) { parent_.mutex_.lock(); }
    ~RAIILock() { parent_.mutex_.unlock(); }
};

#endif // RAIILock_hpp
