/*
    File: client.cpp

    Author: J. Higginbotham
    Department of Computer Science
    Texas A&M University
    Date  : 2016/05/21

    Based on original code by: Dr. R. Bettati, PhD
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
/*
    No additional includes are required
    to complete the assignment, but you're welcome to use
    any that you think would help.
*/
/*--------------------------------------------------------------------------*/

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

#include "reqchannel.hpp"
#include "bounded_buffer.hpp"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

//using counter_t = std::atomic_int;
using counter_t = int;
using counter_vector_t = std::vector<std::shared_ptr<counter_t>>;
using counter_map_t = std::map<std::string, counter_vector_t>;

/*
    This class can be used to write to standard output
    in a multithreaded environment. It's primary purpose
    is printing debug messages while multiple threads
    are in execution.
 */
class atomic_standard_output {
    pthread_mutex_t console_lock;
public:
    atomic_standard_output() { pthread_mutex_init(&console_lock, NULL); }
    ~atomic_standard_output() { pthread_mutex_destroy(&console_lock); }
    void print(std::string s){
        pthread_mutex_lock(&console_lock);
        std::cout << s << std::endl;
        pthread_mutex_unlock(&console_lock);
    }
};

atomic_standard_output threadsafe_standard_output;

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
                     << accumulate(x.second.begin(), x.second.end(), 0, accumulate_shptrs<int, counter_t>);
    }

    return tablebuilder.str();
}

void request_thread_function(bounded_buffer& buf, int count, const std::string& data) {
    for(int i = 0; i < count; i++)
        buf.push_back("data " + data);
}

void event_thread_function(
    std::vector<RequestChannel*>& channels,
    bounded_buffer& request_buffer,
    counter_map_t& counters,
    int max_fd, int n) {

    fd_set read_fds;
    std::vector<std::string> sent_names;
    std::string sent_name;
    unsigned long int send_counter = 0,
                      recv_counter = 0,
                      num_to_send  = NAMES.size() * n,
                      num_channels = channels.size();

    sent_names.resize(num_channels);

    // Initial requests
    for(int i=0; i < num_channels; i++) {
        sent_name = request_buffer.retrieve_front();
        sent_names[i] = sent_name;
        channels[i]->cwrite(sent_name);
        send_counter++;
    }

    while(recv_counter < num_to_send) {
        FD_ZERO(&read_fds);

        for(RequestChannel* chan : channels)
            FD_SET(chan->read_fd(), &read_fds);

        // Wait for ready
        int i = 0;
        int num_ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        for(int j = 0; j < num_channels; j++) {
            if(i == num_ready) break;
            if(FD_ISSET(channels[j]->read_fd(), &read_fds)) {
                std::string response = channels[j]->cread();

                if (!sent_names[j].empty()) {
                    auto search = counters.find(sent_names[j].substr(5));
                    if(search != counters.end()) {
                        *search->second.at(stoi(response) / NUM_BINS) += 1;
                    }
                }

                recv_counter++;

                if(send_counter < num_to_send) {
                    sent_name = request_buffer.retrieve_front();
                    sent_names[j] = sent_name;
                    channels[j]->cwrite(sent_name);
                    send_counter++;
                }
            }
        }
    }
}

static void sigalrm_handler(int sig, siginfo_t *info, void *ucontext) {
    counter_map_t& counters = *(counter_map_t*)info->si_ptr;
    system("clear");

    std::cout << make_histogram_table(counters) << std::endl;
}

/*--------------------------------------------------------------------------*/
/* MAIN FUNCTION */
/*--------------------------------------------------------------------------*/
int main(int argc, char * argv[]) {
    int n = 10; // default number of requests per "patient"
    int b = 50; // default size of request_buffer
    int w = 10; // default number of request channels
    int r = 1;  // default number of timing samples
    std::string outfile;
    bool stats = false;
    int opt = 0;

    while ((opt = getopt(argc, argv, "n:b:w:o:r:hs")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'o':
                outfile.assign(optarg);
                break;
            case 'r':
                r = atoi(optarg);
                if(r < 0) {
                    std::cerr << "r must be 1 or greater" << std::endl;
                    exit(1);
                }
                break;
            case 's':
                stats = true;
                break;
            case 'h':
            default:
                std::cout << "This program can be invoked with the following flags:" << std::endl
                          << " -n [int]  : number of requests per patient" << std::endl
                          << " -b [int]  : size of request buffer" << std::endl
                          << " -w [int]  : number of request channels" << std::endl
                          << " -r [int]  : number of timing samples" << std::endl
                          << " -o [file] : write timing data to a file in CSV format" << std::endl
                          << " -h        : print this message and quit" << std::endl
                          << " -s        : display runtime statistics every 2 seconds" << std::endl
                          << "\nExample: ./client_solution -n 10000 -b 50 -w 120 -o output.csv" << std::endl
                          << "If a given flag is not used, a default value will be given" << std::endl
                          << "to its corresponding variable. If an illegal option is detected," << std::endl
                          << "behavior is the same as using the -h flag." << std::endl;
                exit(0);
        }
    }

    int pid;
    if ((pid = fork()) > 0) {
        std::cout << "n == " << n << std::endl;
        std::cout << "b == " << b << std::endl;
        std::cout << "w == " << w << std::endl;
        std::cout << "r == " << r << std::endl;

        if(stats && r > 1) {
            stats = false;
            std::cout << "Disabling stats because r > 1" << std::endl;
        }

        std::cout << "CLIENT STARTED:" << std::endl;
        std::cout << "Establishing control channel... " << std::flush;
        RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
        std::cout << "done." << std::endl;

        unsigned long int total_nsec = 0;

        for (int j = 0; j < r ; j++) {
            bounded_buffer buf(b);
            counter_map_t counters;

            std::vector<RequestChannel*> channels;
            std::vector<std::thread> request_threads;
            int max_fd = 0;

            for(int i = 0; i < w; i++) {
                std::string s = chan->send_request("newthread");
                RequestChannel* newchan = new RequestChannel(s, RequestChannel::CLIENT_SIDE);
                max_fd = MAX(max_fd, newchan->read_fd());
                channels.push_back(newchan);
            }

            // Timer and signal variables
            timer_t timerid;
            struct sigaction sigact;
            sigevent sigev;
            struct itimerspec itval;

            if (stats) {
                memset(&sigev, 0, sizeof(sigev));
                memset(&sigact, 0, sizeof(sigact));

                sigact.sa_sigaction = sigalrm_handler;
                sigemptyset(&sigact.sa_mask);
                sigact.sa_flags = SA_RESTART | SA_SIGINFO;
                if (sigaction(SIGALRM, &sigact, NULL) == 0)
                    std::cout << "Signal handler registered." << std::endl;
                else
                    std::cerr << "sigaction error: " << strerror(errno) << std::endl;

                sigev.sigev_notify = SIGEV_SIGNAL;
                sigev.sigev_signo = SIGALRM;
                sigev.sigev_value.sival_ptr = (void*)&counters;

                if (timer_create(CLOCK_REALTIME, &sigev, &timerid) == 0) {
                    std::cout << "Stats timer created." << std::endl;

                    memset(&itval, 0, sizeof(itval));
                    itval.it_value.tv_sec = 2;
                    itval.it_interval.tv_sec = 2;

                    if (timer_settime(timerid, 0, &itval, NULL))
                        std::cerr << "time_settime error: " << strerror(errno) << std::endl;
                } else {
                    std::cerr << "timer_create error: " << strerror(errno) << std::endl;
                }
            }


            for (const auto& name : NAMES) {
#ifdef TAMU_COMPUTE
                counter_vector_t counter;
                for (int i = 0; i < NUM_BINS; i++)
                    counter.emplace_back(new counter_t(0));
                counters[name] = counter;
#else
                counters.emplace(name, 0);
                counter_vector_t& counter = counters[name];
                for (int i = 0; i < NUM_BINS; i++)
                    counter.emplace_back(new counter_t(0));
#endif
            }

            auto start = std::chrono::steady_clock::now();

            // Create patient repeater threads
            for(const std::string& name : NAMES)
                request_threads.emplace_back(request_thread_function, std::ref(buf), n, name);

            std::thread event_handler(event_thread_function,
                                    std::ref(channels),
                                    std::ref(buf),
                                    std::ref(counters),
                                    max_fd,
                                    n);

            // wait for requests to complete
            for (auto& th : request_threads)
                th.join();

            // Wait for handler to exit
            event_handler.join();

            // Close channels
            for(RequestChannel* chan : channels) {
                chan->send_request("quit");
                delete chan;
            }

            auto end = std::chrono::steady_clock::now();
            uint64_t nsec = std::chrono::nanoseconds(end - start).count();
            total_nsec += nsec;

            if (stats) {
                if (timer_delete(timerid)) {
                    std::cerr << "timer_delete error: " << strerror(errno) << std::endl;
                } else {
                    std::cout << "Timer disarmed." << std::endl;
                }
            }

            std::cout << "Final counts:" << std::endl
                      << make_histogram_table(counters) << std::endl
                      << "Sample " << j+1 << "/" << r << " took "
                      << nsec << " ns." << std::endl;
        }

        float avg_nsec = (float)total_nsec/r;

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
                os << "n,b,w,r,avg,t" << std::endl;

            os << n << ','
               << b << ','
               << w << ','
               << r << ','
               << avg_nsec << ','
               << total_nsec << std::endl;
        }

        std::cout << "Finished in " << total_nsec << "ns across " << r
                  << " samples (avg. " << avg_nsec << "ns)."
                  << std::endl;

        std::cout << "Sleeping..." << std::endl;
        usleep(10000);
        std::string finale = chan->send_request("quit");
        std::cout << "Finale: " << finale << std::endl;

    } else if (pid == 0) {
        execl("dataserver", NULL);
    }
}
