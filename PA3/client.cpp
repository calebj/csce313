/*
    File: client.cpp

    Completed by: Caleb Johnson
    Department of Computer Science
    Texas A&M University
    Completed : 2018-03-01
    Updated   : 2018-03-03

    Template author: J. Higginbotham
    Department of Computer Science
    Texas A&M University
    Date  : 2016/05/20
    Last Modified : 2017/03/20

    Based on original assignment by: Dr. R. Bettati, PhD
    Department of Computer Science
    Texas A&M University
    Date  : 2013/01/31
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <memory>

// #include <sys/time.h>
#include <chrono>
#include <cassert>
//#include <assert.h>

#include <cmath>
#include <numeric>
#include <algorithm>

#include <list>
#include <vector>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
//#include <errno.h>
#include <cerrno>
#include <unistd.h>
//#include <pthread.h>

#include "reqchannel.hpp"
// #include "SafeCounter.h"
#include "SafeBuffer.hpp"
#include "atomic_stdout.hpp"

// Vector of shared pointers, because there's no copy or move for std::atomic
using counter_vector_t = std::vector<std::shared_ptr<std::atomic_int>>;
using counter_map_t = std::map<std::string, counter_vector_t>;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/*
 * This is a good place to write in storage structs
 * with which to pass parameters to the worker
 * and request thread functions, as well as any other
 * classes you think will be helpful.
 */

atomic_standard_output threadsafe_console_output;

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

const int NUM_BINS = 10,
          MIN_COL_W = 5;

const std::vector<std::string> NAMES = {
    "John Smith",
    "Jane Smith",
    "Joe Smith"
};

/*--------------------------------------------------------------------------*/
/* HELPER FUNCTIONS */
/*
    You may add any helper functions you like, or change the provided functions, 
    so long as your solution ultimately conforms to what is explicitly required
    by the handout.
*/
/*--------------------------------------------------------------------------*/

template <typename T1, typename T2>
T1 accumulate_shptrs(const T1& base, const std::shared_ptr<T2>& elem) {
    return base + *elem;
}

std::string make_histogram_table(counter_map_t& counters) {
    std::stringstream tablebuilder;
    std::map<std::string, int> widths;

    int tmpw;
    for (const auto& x : counters) {
        tmpw = x.second.size();
        widths[x.first] = MAX(tmpw, MIN_COL_W) + 3;
    }

    tablebuilder << std::setw(5) << std::left << "Bin";

    for (const auto& x : counters) {
        tablebuilder << std::setw(widths[x.first]) << std::right << x.first;
    }

    tablebuilder << std::endl;

    for (uint i = 0; i < NUM_BINS; ++i) {
        tablebuilder << std::right << std::setfill('0') << std::setw(2) << std::to_string(i * 10)
                     << "-"
                     << std::right << std::setfill('0') << std::setw(2) << std::to_string((i * 10) + 9)
                     << std::setfill(' ');

        for (const auto& x : counters) {
            tablebuilder << std::right << std::setw(widths[x.first]) << *x.second.at(i);
        }

        tablebuilder << std::endl;
    }

    tablebuilder << std::setw(5) << std::left << "Total";

    for (const auto& x : counters) {
        tablebuilder << std::setw(widths[x.first]) << std::right
                     << accumulate(x.second.begin(), x.second.end(), 0, accumulate_shptrs<int, std::atomic_int>);
    }

    return tablebuilder.str();
}

void request_thread_function(SafeBuffer& buf, int count,
                             const std::string& data) {
    for(int i = 0; i < count; i++)
        buf.push_back(data);
}

void worker_thread_function(
    RequestChannel* chan,
    SafeBuffer& request_buffer,
    counter_map_t& counters)
{

    std::string s = chan->send_request("newthread");
    auto *workerChannel = new RequestChannel(s, RequestChannel::CLIENT_SIDE);

    while(request_buffer.size()) {
        std::string request = request_buffer.retrieve_front();

        if (request == "quit") {
            break;
        } else {
            std::string response = workerChannel->send_request("data " + request);
            auto search = counters.find(request);

            if(search != counters.end()) {
                *search->second.at(stoi(response) / NUM_BINS) += 1;
            }
        }
    }

    workerChannel->send_request("quit");
    delete workerChannel;
}

/*--------------------------------------------------------------------------*/
/* MAIN FUNCTION                                                            */
/*--------------------------------------------------------------------------*/

int main(int argc, char * argv[]) {

    int n   = 100, //default number of requests per "patient"
        w   = 1,   //default number of worker threads
        opt = 0,
        pid;

    std::string outfile;

    while ((opt = getopt(argc, argv, "n:w:o:h")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'o':
                outfile.assign(optarg);
                break;
            case 'h':
            default:
                std::cout << "This program can be invoked with the following flags:" << std::endl
                          << " -n [int]  : number of requests per patient" << std::endl
                          << " -w [int]  : number of worker threads" << std::endl
                          << " -h        : print this message and quit" << std::endl
                          << " -o [file] : write timing data to a file" << std::endl
                          << "(Canonical) example: ./client_solution -n 10000 -w 128" << std::endl
                          << "If a given flag is not used, or given an invalid value," << std::endl
                          << "a default value will be given to the corresponding variable." << std::endl
                          << "If an illegal option is detected, behavior is the same as using the -h flag."
                          << std::endl;
                exit(0);
        }
    }

    if ((pid = fork()) > 0) {
        std::cout << "n == " << n << std::endl;
        std::cout << "w == " << w << std::endl;

        std::cout << "CLIENT STARTED:" << std::endl;
        std::cout << "Establishing control channel... " << std::flush;
        RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
        std::cout << "done." << std::endl;

        SafeBuffer request_buffer;
        counter_map_t counters;

        // Initialize counters with zeros
        for (const auto& name : NAMES) {
            #ifdef TAMU_COMPUTE
            counter_vector_t counter;
            for (int i = 0; i < NUM_BINS; i++)
                counter.emplace_back(new std::atomic_int(0));
            counters[name] = counter;
            #else
            counters.emplace(name, 0);
            counter_vector_t& counter = counters[name];
            for (int i = 0; i < NUM_BINS; i++)
                counter.emplace_back(new std::atomic_int(0));
            #endif
        }

/*--------------------------------------------------------------------------*/
/*  BEGIN CRITICAL SECTION                                                  */
/*--------------------------------------------------------------------------*/

        std::cout << "Populating request buffer... " << std::flush;

        {
            std::vector<std::thread> populate_threads;

            // Create buffer-fill threads
            for (const auto& name : NAMES)
                populate_threads.emplace_back(request_thread_function,
                                            std::ref(request_buffer), n,
                                            name);

            // wait for them to complete
            for (auto& th : populate_threads) 
                th.join();
        }

        std::cout << "done." << std::endl;

        std::cout << "Pushing quit requests... " << std::flush;

        for(int i = 0; i < w; ++i) {
            request_buffer.push_back("quit");
        }
        std::cout << "done." << std::endl;

        std::cout << "Spawning worker threads... " << std::flush;

        /*-------------------------------------------*/
        /* START TIMER HERE */
        /*-------------------------------------------*/

        auto start = std::chrono::steady_clock::now();

        {
            // Populate worker threads
            std::vector<std::thread> worker_threads(w);
            for (int i = 0; i < w; i++) {
                worker_threads[i] = std::thread(worker_thread_function,
                                                chan,
                                                std::ref(request_buffer),
                                                std::ref(counters));
            }

            // wait for them to complete
            for (auto& th : worker_threads) 
                th.join();
        }

        /*-------------------------------------------*/
        /* END TIMER HERE   */
        /*-------------------------------------------*/

        auto end = std::chrono::steady_clock::now();

        uint64_t nsec = std::chrono::nanoseconds(end - start).count();

        std::cout << "workers finished." << std::endl;

/*--------------------------------------------------------------------------*/
/*  END CRITICAL SECTION                                                    */
/*--------------------------------------------------------------------------*/

        std::string histogram_table = make_histogram_table(counters);

        std::cout << "Results for n == " << n << ", w == " << w << std::endl
                  << histogram_table << std::endl
                  << "Time taken: " << nsec << "ns" << std::endl;

        if (!outfile.empty()) {
            // Check if file exists
            bool file_exists = false;
            {
                std::ifstream fin(outfile);
                if (fin) file_exists = true;
            }

            std::cout << "Writing timing data to " << outfile << std::endl;

            auto os = std::ofstream(outfile, std::ios::binary | std::ios::app);

            // If not, then write CSV header
            if (!file_exists)
                os << "n,w,t" << std::endl;

            os << n << ','
               << w << ','
               << nsec << std::endl;
        }

        std::cout << "Sleeping..." << std::endl;
        usleep(100000);

        std::string finale = chan->send_request("quit");
        delete chan;
    }

    else if (pid == 0) {
        execl("dataserver", (char*) nullptr);
    }

    int status;
    waitpid(pid, &status, 0);
}
