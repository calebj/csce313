#ifndef _KERNELSEMAPHORE_HPP
#define _KERNELSEMAPHORE_HPP

#include "Semaphore.hpp"
#include <semaphore.h>

class KernelSemaphore : public SemaphoreBase {
private:
    sem_t *handle;
    std::string name;
public:
    KernelSemaphore(const std::string& name, const unsigned long long& _val = 0) : name(name) {
        if((handle = sem_open(("/" + name).c_str(), O_RDWR | O_CREAT, 0600, _val)) == SEM_FAILED)
            throw semaphore_exception("Error opening semaphore '" + name + "': " + strerror(errno));
    }

    ~KernelSemaphore() {
        sem_close(handle);
    }

    void destroy() override {
        if(sem_unlink(("/" + name).c_str()) == -1)
            throw semaphore_exception("Error destroying semaphore '" + name + "': " + strerror(errno));
    }

    void notify() override {
        if(sem_post(handle) == -1)
            throw semaphore_exception("Error notifying semaphore '" + name + "': " + strerror(errno));
    }

    void wait() override {
        if(sem_wait(handle) == -1)
            throw semaphore_exception("Error waiting semaphore '" + name + "': " + strerror(errno));
    }
};

#endif //_KERNELSEMAPHORE_HPP
