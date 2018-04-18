/*
    File: reqchannel.hpp

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2012/07/11

*/

#ifndef _reqchannel_H_                   // include file only once
#define _reqchannel_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define DEFAULT_KEY "ipc_"

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <exception>
#include <string>
#include <memory>
#include <map>

#include <mqueue.h>
#include <sys/mman.h>
#include <mutex>
#include <semaphore.h>

#include "KernelSemaphore.hpp"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

class sync_lib_exception : public std::exception {
	std::string err = "failure in sync library";
	
public:
	sync_lib_exception() {}
	sync_lib_exception(std::string msg) : err(msg) {}
	virtual const char* what() const throw() {
		return err.c_str();
	}
};

/*--------------------------------------------------------------------------*/
/* FORWARDS                                                                 */
/*--------------------------------------------------------------------------*/

class RequestChannel;
using RequestChannelPtr = std::shared_ptr<RequestChannel>;

/*--------------------------------------------------------------------------*/
/* RequestChannel class                                                     */
/*--------------------------------------------------------------------------*/

class RequestChannel {
public:
	typedef enum {SERVER_SIDE, CLIENT_SIDE} Side;
	typedef enum {READ_MODE, WRITE_MODE} Mode;
    typedef enum {FIFO, MQ, SHM} Type;
    static std::map<const std::string, Type> TypeMap;
protected:
    const Type instance_type;
    std::string  base_key = "";
	std::string   my_name = "";
	std::string side_name = "";
	Side my_side;
	
	/*	Locks used to keep the dataserver from dropping requests.	*/
    std::mutex read_lock, write_lock, send_request_lock;

public:
    /* Creates a "local copy" of the channel specified by the given name.
     If the channel does not exist, the associated IPC mechanisms are
     created. If the channel exists already, this object is associated with the channel.
     The channel has two ends, which are conveniently called "SERVER_SIDE" and "CLIENT_SIDE".
     If two processes connect through a channel, one has to connect on the server side
     and the other on the client side. Otherwise the results are unpredictable.

     NOTE: If the creation of the request channel fails (typically happens when too many
     request channels are being created) and error message is displayed, and the program
     unceremoniously exits.

     NOTE: It is easy to open too many request channels in parallel. In most systems,
     limits on the number of open files per process limit the number of established
     request channels to 125. */
	RequestChannel(const RequestChannel::Type& _type,
                   const std::string& _name,
                   const Side& _side,
                   const std::string& _key = DEFAULT_KEY);

    /* Destructor of the local copy of the bus. By default, the server side cleans up any IPC
     mechanisms associated with the channel. */
	virtual ~RequestChannel();

	// === STATIC METHODS ===

	/* A factory constructor method */
	static RequestChannelPtr get_channel(const Type& type,
                                         const std::string& name,
                                         const Side& side,
                                         const std::string& key = DEFAULT_KEY);

	/* Returns the name corresponding to a type */
	static const std::string get_typename(const Type& type);

    /* Returns the type corresponding to a type name */
    static const Type get_type(const std::string& type_name);

    /* Send a string over the channel and wait for a reply. */
	std::string send_request(const std::string& _request);

	// === GETTER FUNCTIONS ===

    /* Returns the name corresponding to the type of the current instance */
    const std::string type_name() const { return get_typename(instance_type); }

    /* Returns the name of the request channel. */
    const std::string name() const { return my_name; }

    /* Returns the base key */
    const std::string key() const { return base_key; }

    /* Return the type (enum) of an instance. */
    const Type& type() const { return instance_type; }

    /* Returns the "base name", e.g. ipc_control */
    const std::string handle_basename() const;

    /* Returns the proper name of one side of the IPC channel, e.g. ipc_control1 */
    const std::string handle_name(const Mode &_mode) const;

    // === VIRTUAL FUNCTIONS ===

    /* Blocking read of data from the channel. Returns a string of characters read from the channel. */
	virtual std::string cread() = 0;

    /* Write the data to the channel. The function returns the number of characters written to the channel. */
	virtual ssize_t cwrite(const std::string &_msg) = 0;
};

class FIFORequestChannel : public RequestChannel {
private:
    int wfd;
    int rfd;

    void open_read_pipe(const char *_pipe_name);
    void open_write_pipe(const char *_pipe_name);
    bool read_pipe_opened = false;
    bool write_pipe_opened = false;

public:

    // === CON/DESTRUCTORS ===

	FIFORequestChannel(const std::string& _name, const Side& _side, const std::string& _key);
    ~FIFORequestChannel();

    // === OVERRIDDEN FUNCTIONS ===

    std::string cread() override;
    ssize_t cwrite(const std::string &_msg) override;

    // === FIFO-SPECIFIC FUNCTIONS ===

    /* Returns the file descriptor used to read from the channel. */
    const int read_fd() const { return rfd; }

    /* Returns the file descriptor used to write to the channel. */
    const int write_fd() const { return wfd; }
};

class MQRequestChannel : public RequestChannel {
private:
    mqd_t send_mq;
    mqd_t recv_mq;
public:

    // === CON/DESTRUCTORS ===

    MQRequestChannel(const std::string& _name, const Side& _side, const std::string& _key);
    ~MQRequestChannel();

    // === OVERRIDDEN FUNCTIONS ===

    std::string cread() override;
    ssize_t cwrite(const std::string &_msg) override;
};

class SHMRequestChannel : public RequestChannel {
private:
    int recv_shm_fd, send_shm_fd;
    //sem_t *read_sem, *write_sem, *read_mutex, *write_mutex;
    KernelSemaphore read_sem, write_sem, read_mutex, write_mutex;
    size_t recv_mmap_size, send_mmap_size;
    void *recv_mmap_handle, *send_mmap_handle;
public:

    // === CON/DESTRUCTORS ===

    SHMRequestChannel(const std::string& _name, const Side& _side, const std::string& _key);
    ~SHMRequestChannel();

    // === OVERRIDDEN FUNCTIONS ===

    std::string cread() override;
    ssize_t cwrite(const std::string &_msg) override;
};

#endif