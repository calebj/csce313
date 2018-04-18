/* 
    File: dataserver.C

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2012/07/16

    Dataserver main program for MP3 in CSCE 313
*/

/*--------------------------------------------------------------------------*/
/* DEFINES                                                                  */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES                                                                 */
/*--------------------------------------------------------------------------*/

#include <cassert>
#include <cstring>
#include <sstream>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "reqchannel.hpp"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES                                                          */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS                                                                */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* VARIABLES                                                                */
/*--------------------------------------------------------------------------*/

std::mutex channel_mutex;
static unsigned long int nthreads = 0;
std::vector<std::thread> threads;

/*--------------------------------------------------------------------------*/
/* FORWARDS                                                                 */
/*--------------------------------------------------------------------------*/

void handle_process_loop(RequestChannelPtr _channel);

/*--------------------------------------------------------------------------*/
/* LOCAL FUNCTIONS -- SUPPORT FUNCTIONS                                     */
/*--------------------------------------------------------------------------*/

	/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* LOCAL FUNCTIONS -- THREAD FUNCTIONS                                      */
/*--------------------------------------------------------------------------*/

//void *handle_data_requests(void * args) {
//	auto *data_channel =  (RequestChannel*)args;
//
//	// -- Handle client requests on this channel.
//	handle_process_loop(*data_channel);
//
//	// -- Client has quit. We remove channel.
//	delete data_channel;
//
//	return nullptr;
//}

void handle_process_loop(RequestChannelPtr _channel);

/*--------------------------------------------------------------------------*/
/* LOCAL FUNCTIONS -- INDIVIDUAL REQUESTS                                   */
/*--------------------------------------------------------------------------*/

void process_hello(RequestChannelPtr _channel, const std::string& _request) {
	_channel->cwrite("hello to you too");
}

void process_data(RequestChannelPtr _channel, const std::string&  _request) {
	usleep(1000 + (rand() % 5000));
	//_channel.cwrite("here comes data about " + _request.substr(4) + ": " + std::to_string(random() % 100));
	_channel->cwrite(std::to_string(rand() % 100));
}

void process_newthread(RequestChannelPtr _channel, const std::string& _request) {
    channel_mutex.lock();
    nthreads++;

    // -- Name new data channel
    std::string new_channel_name = "data" + std::to_string(nthreads) + "_";
    // std::cerr << "new channel name = " << new_channel_name << std::endl;

    // -- Pass new channel name back to client

    _channel->cwrite(new_channel_name);
    channel_mutex.unlock();

	try {
        // -- Construct new data channel (pointer to be passed to thread function)
		RequestChannelPtr data_channel = RequestChannel::get_channel(_channel->type(),
                                                                     new_channel_name,
                                                                     RequestChannel::SERVER_SIDE,
                                                                     _channel->key());

		// -- Create new thread to handle request channel
        threads.emplace_back(handle_process_loop, data_channel);
	} catch (sync_lib_exception sle) {
		perror(std::string(sle.what()).c_str());
	}
}

/*--------------------------------------------------------------------------*/
/* LOCAL FUNCTIONS -- THE PROCESS REQUEST LOOP                              */
/*--------------------------------------------------------------------------*/

void process_request(RequestChannelPtr _channel, const std::string& _request) {
    if (_request.compare(0, 4, "data") == 0) {
        process_data(_channel, _request);
    } else if (_request.compare(0, 9, "newthread") == 0) {
        process_newthread(_channel, _request);
    } else if (_request.compare(0, 5, "hello") == 0) {
		process_hello(_channel, _request);
	} else {
		_channel->cwrite("unknown request");
	}
}

void handle_process_loop(RequestChannelPtr _channel) {
	for(;;) {
		// std::cout << "Reading next request from channel (" << _channel->name() << ") ..." << std::flush;
		// std::cout << std::flush;
		std::string request = _channel->cread();
		// std::cout << " done (" << _channel->name() << ")." <<std::endl;
		// std::cout << "New request is " << request <<std::endl;

		if (request == "quit") {
			_channel->cwrite("bye");
			//usleep(10000);          // give the other end a bit of time.
			break;                  // break out of the loop;
		}

		process_request(_channel, request);
	}
}

/*--------------------------------------------------------------------------*/
/* MAIN FUNCTION                                                            */
/*--------------------------------------------------------------------------*/

int main(int argc, char * argv[]) {
    if (argc != 3) {
        std::cerr << "USAGE: " << argv[0] << " " << "[IPC_METHOD] [IPC_KEY_BASE]" << " (got " << argc << ")" << std::endl;
        exit(1);
    }

    std::string ipc_method(argv[1]), ipc_key_base(argv[2]);
    RequestChannel::Type ipc_type;

    try {
        ipc_type = RequestChannel::get_type(ipc_method);
    } catch (const sync_lib_exception& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

//    std::cerr << "SERVER: Establishing control channel... " << std::endl;

	RequestChannelPtr control_channel = RequestChannel::get_channel(ipc_type, "control", RequestChannel::SERVER_SIDE, ipc_key_base);
//	std::cout << "done.\n" << std::endl;

	handle_process_loop(control_channel);
	for(std::thread& t : threads) t.join();
}