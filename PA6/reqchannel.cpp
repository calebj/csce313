/* 
    File: requestchannel.cpp

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2012/07/11

*/

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <algorithm>

#include "reqchannel.hpp"

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

const bool VERBOSE = false;

const int MAX_MESSAGE = 255;

std::map<const std::string, RequestChannel::Type> RequestChannel::TypeMap = {
        {"FIFO", FIFO},
        {"MQ",   MQ},
        {"SHM",  SHM}
};

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

// (none)

/*--------------------------------------------------------------------------*/
/* Shared/base methods                                                      */
/*--------------------------------------------------------------------------*/

RequestChannel::RequestChannel(const RequestChannel::Type& _type, const std::string& _name, const Side& _side, const std::string& _key)
 : instance_type(_type), my_name(_name), my_side(_side), base_key(_key),
   side_name((_side == RequestChannel::SERVER_SIDE) ? "SERVER" : "CLIENT")
{
}

RequestChannel::~RequestChannel() {
    if(VERBOSE) std::cout << my_name << ":" << side_name << ": closing..." << std::endl;
}

const std::string RequestChannel::handle_basename() const {
    return base_key + my_name;
}

const std::string RequestChannel::handle_name(const Mode &_mode) const {
    std::string name = handle_basename();

    if (my_side == CLIENT_SIDE) {
        if (_mode == READ_MODE)
            name += "1";
        else
            name += "2";
    } else {
        /* SERVER_SIDE */
        if (_mode == READ_MODE)
            name += "2";
        else
            name += "1";
    }

    return name;
}

std::string RequestChannel::send_request(const std::string& _request) {
    std::lock_guard<std::mutex> lock(send_request_lock);

    if(cwrite(_request) < 0) {
        return "ERROR";
    }

    std::string s = cread();
    return s;
}

const std::string RequestChannel::get_typename(const Type& type) {

    for(const auto& ntp : TypeMap) {
        if(ntp.second == type)
            return ntp.first;
    }

    throw std::out_of_range("Unknown type: " + std::to_string(type));
}

const RequestChannel::Type RequestChannel::get_type(const std::string& type_name) {
    std::string typename_upper(type_name);
    std::transform(typename_upper.begin(), typename_upper.end(), typename_upper.begin(), ::toupper);

    try {
        return RequestChannel::TypeMap.at(typename_upper);
    } catch (const std::out_of_range&) {
        throw sync_lib_exception("Unknown IPC type: " + type_name);
    }
}

RequestChannelPtr RequestChannel::get_channel(const RequestChannel::Type& type,
                                              const std::string& name,
                                              const RequestChannel::Side& side,
                                              const std::string& key) {
    switch(type) {
        case RequestChannel::Type::FIFO:
            return RequestChannelPtr(new FIFORequestChannel(name, side, key));
        case RequestChannel::Type::MQ:
            return RequestChannelPtr(new MQRequestChannel(name, side, key));
        case RequestChannel::Type::SHM:
            return RequestChannelPtr(new SHMRequestChannel(name, side, key));
        default:
            throw sync_lib_exception("Invalid request channel type");
    }
}

/*--------------------------------------------------------------------------*/
/* FIFO implementation                                                      */
/*--------------------------------------------------------------------------*/

void FIFORequestChannel::open_write_pipe(const char *_pipe_name) {

	if(write_pipe_opened) close(wfd);

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": mkfifo write pipe" << std::endl;

	if (mkfifo(_pipe_name, 0600) < 0) {
		if (errno != EEXIST) {
			int prev_errno = errno;
			if(read_pipe_opened) close(rfd);
			remove(handle_name(READ_MODE).c_str());
			remove(handle_name(WRITE_MODE).c_str());
			errno = prev_errno;
			throw sync_lib_exception(my_name + ":" + side_name + ": error creating pipe for writing");
		}
	}

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": open write pipe" << std::endl;

	wfd = open(_pipe_name, O_WRONLY);
	if (wfd < 0) {
		int prev_errno = errno;
		if(read_pipe_opened) close(rfd);
		remove(handle_name(READ_MODE).c_str());
		remove(handle_name(WRITE_MODE).c_str());
		errno = prev_errno;
		throw sync_lib_exception(my_name + ":" + side_name + ": error opening pipe for writing");
	}

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": done opening write pipe" << std::endl;
	write_pipe_opened = true;
}

void FIFORequestChannel::open_read_pipe(const char *_pipe_name) {

	if(read_pipe_opened) close(rfd);
	
	if(VERBOSE) std::cout << my_name << ":" << side_name << ": mkfifo read pipe" << std::endl;

	if (mkfifo(_pipe_name, 0600) < 0) {
		if (errno != EEXIST) {
			int prev_errno = errno;
			if(write_pipe_opened) close(wfd);
			remove(handle_name(READ_MODE).c_str());
			remove(handle_name(WRITE_MODE).c_str());
			errno = prev_errno;
			throw sync_lib_exception(my_name + ":" + side_name + ": error creating pipe for reading");
		}
	}

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": open read pipe" << std::endl;

	rfd = open(_pipe_name, O_RDONLY);
	if (rfd < 0) {
		int prev_errno = errno;
		if(write_pipe_opened) close(wfd);
		remove(handle_name(READ_MODE).c_str());
		remove(handle_name(WRITE_MODE).c_str());
		errno = prev_errno;
		throw sync_lib_exception(my_name + ":" + side_name + ": error opening pipe for reading");
	}

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": done opening read pipe" << std::endl;
	read_pipe_opened = true;
}

FIFORequestChannel::FIFORequestChannel(const std::string& _name, const Side& _side, const std::string& _key)
 : RequestChannel(FIFO, _name, _side, _key) {
	//Necessary for proper error handling
	sigset_t sigpipe_set;
	sigemptyset(&sigpipe_set);

	if(sigaddset(&sigpipe_set, SIGPIPE) < 0) {
		throw sync_lib_exception(my_name + ":" + side_name + ": failed on sigaddset");
	}
	
	if((errno = pthread_sigmask(SIG_SETMASK, &sigpipe_set, nullptr)) != 0) {
		throw sync_lib_exception(my_name + ":" + side_name + ": failed on pthread_sigmask");
	}
	
	if (_side == SERVER_SIDE) {
		open_write_pipe(handle_name(WRITE_MODE).c_str());
		open_read_pipe(handle_name(READ_MODE).c_str());
	}
	else {
		open_read_pipe(handle_name(READ_MODE).c_str());
		open_write_pipe(handle_name(WRITE_MODE).c_str());
	}
}

FIFORequestChannel::~FIFORequestChannel() {
	close(wfd);
	close(rfd);

	if (my_side == RequestChannel::SERVER_SIDE) {
		if(VERBOSE) {
            std::cout << "RequestChannel:" << my_name << ":" << side_name
                      << "close IPC mechanisms on server side for channel "
                      << my_name << std::endl;
        }

		/* Delete the underlying IPC mechanisms. */
		if (remove(handle_name(READ_MODE).c_str()) != 0 && errno != ENOENT) {
            std::cerr << my_name + ": Error deleting pipe read pipe: " << strerror(errno) << std::endl;
		}

		if (remove(handle_name(WRITE_MODE).c_str()) != 0 && errno != ENOENT) {
            std::cerr << my_name + ": Error deleting pipe write pipe: " << strerror(errno) << std::endl;
		}
	}
}

std::string FIFORequestChannel::cread() {

    std::lock_guard<std::mutex> lock(read_lock);
	
	char buf[MAX_MESSAGE];
	memset(buf, '\0', MAX_MESSAGE);

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": reading..." << std::endl;
	
	ssize_t read_return_value;
	if ((read_return_value = read(rfd, buf, MAX_MESSAGE)) <= 0) {
		if(read_return_value < 0) {
            throw sync_lib_exception(my_name + ":" + side_name + ": error reading from pipe: " + strerror(errno));
		}
		else {
            throw sync_lib_exception(my_name + ":" + side_name + ": cread: broken/closed pipe detected!");
		}
	}

	std::string s = buf;

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": reads [" << buf << "]" << std::endl;

	return s;
}

ssize_t FIFORequestChannel::cwrite(const std::string &_msg) {

	if (_msg.length() >= MAX_MESSAGE) {
		if(VERBOSE) std::cerr << my_name << ":" << side_name << "Message too long for Channel!" << std::endl;
		return -1;
	}

    std::lock_guard<std::mutex> lock(write_lock);

	if(VERBOSE) std::cout << my_name << ":" << side_name << ": writing [" << _msg << "]" << std::endl;

	const char *s = _msg.c_str();

	ssize_t write_return_value;
	if ((write_return_value = write(wfd, s, strlen(s) + 1)) < 0) {
		// EPIPE is part of the normal error-handling process for
		// RequestChannels that are successfully constructed,
		// but that don't have threads enough threads created for them.

		if(errno != EPIPE) {
            throw sync_lib_exception(my_name + ":" + side_name + ": error writing to pipe: " + strerror(errno));
		} else {
            throw sync_lib_exception(my_name + ":" + side_name + ": cwrite: broken/closed pipe detected!");
		}
	}

    if(VERBOSE) std::cout << my_name << ":" << side_name << ": done writing." << std::endl;
	
	return write_return_value;
}

/*--------------------------------------------------------------------------*/
/* MQ implementation                                                        */
/*--------------------------------------------------------------------------*/

MQRequestChannel::MQRequestChannel(const std::string& _name, const Side& _side, const std::string& _key)
        : RequestChannel(MQ, _name, _side, _key) {

    struct mq_attr channel_mq_attr;
    memset(&channel_mq_attr, 0, sizeof(mq_attr));
    channel_mq_attr.mq_msgsize = MAX_MESSAGE;
    channel_mq_attr.mq_maxmsg = 4;

    if((send_mq = mq_open(("/" + handle_name(WRITE_MODE)).c_str(), O_RDWR | O_CREAT, 0600, &channel_mq_attr)) == -1)
        throw sync_lib_exception(my_name + ": Error opening send mq: " + strerror(errno));

    if((recv_mq = mq_open(("/" + handle_name(READ_MODE)).c_str(), O_RDWR | O_CREAT, 0600, &channel_mq_attr)) == -1)
        throw sync_lib_exception(my_name + ": Error opening recv mq: " + strerror(errno));
}

MQRequestChannel::~MQRequestChannel() {
    mq_close(send_mq);
    mq_close(recv_mq);

    if (my_side == RequestChannel::SERVER_SIDE) {
        if(VERBOSE) {
            std::cout << "RequestChannel:" << my_name << ":" << side_name
                      << "close IPC mechanisms on server side for channel "
                      << my_name << std::endl;
        }

        /* Delete the underlying IPC mechanisms. */
        if (mq_unlink(("/" + handle_name(READ_MODE)).c_str()) != 0 && errno != ENOENT) {
            std::cerr << my_name + ": Error deleting read mq: " << strerror(errno) << std::endl;
        }

        if (mq_unlink(("/" + handle_name(WRITE_MODE)).c_str()) != 0 && errno != ENOENT) {
            std::cerr << my_name + ": Error deleting write mq: " << strerror(errno) << std::endl;
        }
    }

}

std::string MQRequestChannel::cread() {
    std::lock_guard<std::mutex> lock(read_lock);

    char buf[MAX_MESSAGE + 1];
    memset(buf, '\0', MAX_MESSAGE + 1);

    if(mq_receive(recv_mq, buf, MAX_MESSAGE, nullptr) < 0)
        throw sync_lib_exception(my_name + ":" + side_name + ": error reading from mq: " + strerror(errno));

    std::string ret(buf);
    return ret;
}

ssize_t MQRequestChannel::cwrite(const std::string& _msg) {
    std::lock_guard<std::mutex> lock(write_lock);
    if(mq_send(send_mq, _msg.c_str(), MAX_MESSAGE, 1) != 0)
        throw sync_lib_exception(my_name + ":" + side_name + ": error writing to mq: " + strerror(errno));
    return _msg.size();
}

/*--------------------------------------------------------------------------*/
/* SHM implementation                                                       */
/*--------------------------------------------------------------------------*/

SHMRequestChannel::SHMRequestChannel(const std::string& _name, const Side& _side, const std::string& _key)
 : RequestChannel(SHM, _name, _side, _key),
   send_mmap_size(MAX_MESSAGE + 1),
   recv_mmap_size(MAX_MESSAGE + 1),
   read_sem(handle_name(READ_MODE) + "_sem"),
   write_sem(handle_name(WRITE_MODE) + "_sem"),
   read_mutex(handle_name(READ_MODE) + "_mtx"),
   write_mutex(handle_name(WRITE_MODE) + "_mtx")
{

    // === OPEN SEGMENTS ===

    if((send_shm_fd = shm_open(("/" + handle_name(WRITE_MODE)).c_str(), O_RDWR | O_CREAT, 0600)) == -1)
        throw sync_lib_exception(my_name + ": Error opening send shm: " + strerror(errno));

    if((recv_shm_fd = shm_open(("/" + handle_name(READ_MODE)).c_str(), O_RDWR | O_CREAT, 0600)) == -1)
        throw sync_lib_exception(my_name + ": Error opening recv shm: " + strerror(errno));

    // === TRUNCATE SEGMENTS ===

    if(ftruncate(send_shm_fd, send_mmap_size) == -1)
        throw sync_lib_exception(my_name + ": Error ftruncate send sem: " + strerror(errno));

    if(ftruncate(recv_shm_fd, recv_mmap_size) == -1)
        throw sync_lib_exception(my_name + ": Error ftruncate recv sem: " + strerror(errno));

    // === MAP SEGMENTS ===

    if((send_mmap_handle = mmap(nullptr, send_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, send_shm_fd, 0)) == MAP_FAILED)
        throw sync_lib_exception(my_name + ": Error mapping write memory: " + strerror(errno));

    if((recv_mmap_handle = mmap(nullptr, recv_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, recv_shm_fd, 0)) == MAP_FAILED)
        throw sync_lib_exception(my_name + ": Error mapping read memory: " + strerror(errno));

    if (my_side == RequestChannel::SERVER_SIDE) {
        read_mutex.notify();
        write_mutex.notify();
    }
}

SHMRequestChannel::~SHMRequestChannel() {

    // === UNMAP SEGMENTS ===

    munmap(send_mmap_handle, send_mmap_size);
    munmap(recv_mmap_handle, recv_mmap_size);

    // === CLOSE SEGMENTS ===

    close(send_shm_fd);
    close(recv_shm_fd);

    // === CLOSE SEMAPHORES ===

    if (my_side == RequestChannel::SERVER_SIDE) {
        if (VERBOSE) {
            std::cout << "RequestChannel:" << my_name << ":" << side_name
                      << "close IPC mechanisms on server side for channel "
                      << my_name << std::endl;
        }

        // === DELETE SEGMENTS ===

        if (shm_unlink(("/" + handle_name(READ_MODE)).c_str()) != 0 && errno != ENOENT) {
            std::cerr << my_name + ": Error deleting read shm: " << strerror(errno) << std::endl;
        }

        if (shm_unlink(("/" + handle_name(WRITE_MODE)).c_str()) != 0 && errno != ENOENT) {
            std::cerr << my_name + ": Error deleting write shm: " << strerror(errno) << std::endl;
        }

        // === DELETE SEMAPHORES ===
        read_sem.destroy();
        write_sem.destroy();
        read_mutex.destroy();
        write_mutex.destroy();

    }
}

std::string SHMRequestChannel::cread() {
    std::lock_guard<std::mutex> lock(read_lock);
    std::string ret;

    read_sem.wait();
    read_mutex.wait();

    ret.assign((char*)recv_mmap_handle);

    read_mutex.notify();

    return ret;
}

ssize_t SHMRequestChannel::cwrite(const std::string& _msg) {
    std::lock_guard<std::mutex> lock(write_lock);

    write_mutex.wait();

    ssize_t copy_len = MIN(_msg.size()+1, MAX_MESSAGE);
    strncpy((char*)send_mmap_handle, _msg.c_str(), copy_len);

    write_mutex.notify();
    write_sem.notify();

    return copy_len;
}