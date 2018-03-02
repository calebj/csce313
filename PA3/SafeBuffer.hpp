//
//  SafeBuffer.h
//
//  Created by Caleb Johnson on 2018-03-01.
//  Loosely based on the skeleton by Joshua Higginbotham.
//

#ifndef SafeBuffer_h
#define SafeBuffer_h

#include <queue>
#include <string>

#include "RAIILock.hpp"

template <typename T>
class SafeBufferTemplate : public RAIILockBase {
    std::queue<T> data;

public:
    int size();
    void push_back(T obj);
    T retrieve_front();
};

template <typename T>
int SafeBufferTemplate<T>::size() {
    RAIILock lock(*this);
    return data.size();
}

template <typename T>
void SafeBufferTemplate<T>::push_back(T obj) {
    RAIILock lock(*this);
    data.push(obj);
}

template <typename T>
T SafeBufferTemplate<T>::retrieve_front() {
    RAIILock lock(*this);
    T obj = data.front();
    data.pop();
    return obj;
}

using SafeBuffer = SafeBufferTemplate<std::string>;

#endif /* SafeBuffer_h */
