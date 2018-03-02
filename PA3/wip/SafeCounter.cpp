#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include "SafeCounter.h"

SafeCounter::SafeCounter(uint bins) : num_bins_(bins) {
    counters_.resize(bins);
    for (int i = 0; i < bins ; i++)
        counters_[i] = std::make_shared<std::atomic_int>(0);
}

std::ostream& operator<< (std::ostream& os, const SafeCounterMap& counter) {
/*
        std::stringstream tablebuilder;

        tablebuilder << std::setw(21) << std::right << name1;
        tablebuilder << std::setw(15) << std::right << name2;
        tablebuilder << std::setw(15) << std::right << name3 << std::endl;

        for (uint i = 0; i < data1.size(); ++i) {
            tablebuilder << std::setw(6) << std::left
            << std::to_string(i * 10) + "-" + std::to_string((i * 10) + 9)
            << std::setw(15) << std::right << *data1.at(i)
            << std::setw(15) << std::right << *data2.at(i)
            << std::setw(15) << std::right << *data3.at(i)
            << std::endl;
        }

        tablebuilder << std::setw(6) << std::left << "Total"
        << std::setw(15) << std::right
        << accumulate(data1.begin(), data1.end(), 0, accumulate_shptrs<int, std::atomic_int>)
        << std::setw(15) << std::right
        << accumulate(data2.begin(), data2.end(), 0, accumulate_shptrs<int, std::atomic_int>)
        << std::setw(15) << std::right
        << accumulate(data3.begin(), data3.end(), 0, accumulate_shptrs<int, std::atomic_int>)
        << std::endl;

        return tablebuilder.str();
*/
    return os;
}

uint SafeCounterMap::add_counter(const std::string &name) {
    auto existing = names_.find(name);

    if(existing == names_.end()) {
        return existing->second;
    }

    uint new_index = counters_.size();
    counters_.emplace_back(num_bins_);
    names_[name] = new_index;
    return new_index;
}
