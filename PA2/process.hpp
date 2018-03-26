#ifndef PROCESS_H
#define PROCESS_H

#include <array>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <utility>
#include <deque>
#include <memory>
#include <deque>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

// Forward declarations
class ProcessFlow;
class ProcessController;

using dirstack_t = std::deque<std::string>;

// Represents a process
class Subprocess {
    friend class ProcessController;
    friend class ProcessFlow;
public:
    pid_t run(int fd_in=STDIN_FILENO, int fd_out=STDOUT_FILENO);
    void exec(int fd_in=STDIN_FILENO, int fd_out=STDOUT_FILENO);

    const pid_t& get_pid() const { return _pid; }
    const std::string& get_command() const { return _command; }
    const std::vector<std::string>& get_args() const { return _args; }
    const std::vector<std::string>& get_tokens() const { return _tokens; }
    ProcessFlow& get_flow() const { return *_flow; }

    char** get_argv() const;
    void free_argv(char** argv) const;
private:
    pid_t _pid = -1;
    ProcessFlow* _flow;
    std::string _command;
    std::vector<std::string> _args, _tokens;
};

// Represents one or more processes that form a pipeline of data
class ProcessFlow {
    friend class ProcessController;
public:
    void add_process(Subprocess& proc);
    void set_input(const std::string& filename);
    void set_output(const std::string& filename, bool append = false);

    ProcessController& get_controller() const { return *_controller; }
    const std::vector<Subprocess>& get_flow() const { return _flow; }

    bool prepare();
    void cleanup();
    pid_t run(bool background = false);

    ~ProcessFlow();
private:
    ProcessController* _controller;
    std::string _input_filename,
                _output_filename;
    FILE *_input_handle = NULL,
         *_output_handle = NULL;
    bool _output_append = false,
         _detach_all = false,
         _ready = false;
    std::vector<Subprocess> _flow;
};

// Represents a "controller harness" for the various processes and jobs
class ProcessController {
public:
    void parse(const std::vector<std::string>& tokens);
    void reset_pending() { _pending.clear(); }

    void enqueue_job(std::shared_ptr<ProcessFlow> flow, bool background = false);
    std::shared_ptr<ProcessFlow> get_job(int job_id) const;
    const std::vector<std::shared_ptr<ProcessFlow>>& get_jobs() const { return _jobs; }
    dirstack_t& get_dirstack() { return _dirstack; }
    const bool is_background_pid(int pid) const;

    void run();

    void update_child(pid_t pid, int status);


private:
    // Holds jobs while parsing. Second value in pair is background flag.
    std::deque<std::pair<std::shared_ptr<ProcessFlow>, bool>> _pending;
    dirstack_t _dirstack;

    std::vector<std::shared_ptr<ProcessFlow>>
        _flows, // Foreground commands in order
        _jobs;  // Background jobs indexed by ID - 1
};

////////////////
// Exceptions //
////////////////

class process_exception : public std::runtime_error {
public:
    process_exception(const std::string& arg) : std::runtime_error(arg) {};
    ~process_exception() throw() {};
};

class process_badtoken : public process_exception {
public:
    process_badtoken(const std::string& token)
    : process_exception("unexpected token: " + token), token(token) {};
    process_badtoken(const std::string& token, const std::string& after)
    : process_exception("unexpected token: " + token + "(after" + after + ")"),
      token(token), after(after) {};
    std::string token;
    std::string after;
    ~process_badtoken() throw() {};
};

class process_filenotfound : public process_exception {
public:
    process_filenotfound(const std::string& filename)
    : process_exception("cannot find file: " + filename), filename(filename) {};
    std::string filename;
    ~process_filenotfound() throw() {};
};

class process_jobnotfound : public process_exception {
public:
    process_jobnotfound(const int& job_id)
    : process_exception("no such job: " + std::to_string(job_id)),
      job_id(job_id) {};
    int job_id;
    ~process_jobnotfound() throw() {};
};

#endif
