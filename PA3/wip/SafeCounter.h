#ifndef SAFECOUNTER_H
#define SAFECOUNTER_H

#include <atomic>
#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <memory>

class SafeCounter;

class SafeCounterMap {
    friend std::ostream& operator<< (std::ostream& os, const SafeCounterMap& counter);
public:
    SafeCounterMap(uint bins = 10) : num_bins_(bins) {}

    // Prevent copying or moving
    SafeCounterMap(const SafeCounterMap&) = delete;
    SafeCounterMap& operator=(const SafeCounterMap&) = delete;

    SafeCounter& get_counter(const std::string &name) { return counters_.at(get_counter_id(name)); }
    SafeCounter& operator[] (const std::string& name) { return get_counter(name); }

    uint get_counter_id(const std::string &name) const { return names_.at(name); }
    uint add_counter(const std::string &name);

private:
    std::map<std::string, uint> names_;
    std::vector<std::vector<std::shared_ptr<std::atomic_int>>> counters_;
    uint num_bins_;
};

class SafeCounterProxy {
    friend class SafeCounterMap;
public:
    std::atomic_int& get_counter(uint index) { return *counters_.at(index); }
    std::atomic_int& operator[] (uint index) { return *counters_.at(index); }

private:
    SafeCounterProxy(SafeCounter& parent) : parent_(parent);
};

#endif //SAFECOUNTER_H
