#include <set>
#include <iostream>
#include <utility>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "process.hpp"
#include "builtins.hpp"

////////////////
// Subprocess //
////////////////

char** Subprocess::get_argv() const {
    char ** argv = new char*[_args.size() + 2];

    argv[0] = new char[_command.size() + 1];
    strcpy(argv[0], _command.c_str());

    argv[_args.size() + 1] = NULL;

    for(size_t i = 0; i < _args.size(); i++){
        argv[i + 1] = new char[_args[i].size() + 1];
        strcpy(argv[i + 1], _args[i].c_str());
    }

    return argv;
}

void Subprocess::free_argv(char** argv) const {
    for(size_t i = 0; i < _args.size() + 1; i++) {
        delete [] argv[i];
    }

    delete [] argv;
}

void Subprocess::exec(int fd_in, int fd_out) {
    // Replace or close stdin
    if (fd_in == -1) {
        close(STDIN_FILENO);
    } else if (fd_in != STDIN_FILENO) {
        if (dup2(fd_in, STDIN_FILENO) == -1) {
            std::cerr << "failed to dup2 stdin" << std::endl;
            exit(EXIT_FAILURE);
        }
        close(fd_in);
    }

    // Replace or close stdout
    if (fd_out == -1) {
        close(STDOUT_FILENO);
    } else if (fd_out != STDOUT_FILENO) {
        if (dup2(fd_out, STDOUT_FILENO) == -1) {
            std::cerr << "failed to dup2 stdout" << std::endl;
            exit(EXIT_FAILURE);
        }
        close(fd_out);
    }

    // Set up pathname and argv for execvp()
    const char* pathname = _command.data();
    char** argv = get_argv();

    // Call execvp()
    execvp(pathname, argv);

    // Why are we still here?
    std::cerr << "Command failed to execute: "
    << _command << std::endl;

    free_argv(argv);
}

pid_t Subprocess::run(int fd_in, int fd_out) {
    pid_t pid; // Local, can be 0 or actual PID

    const auto it = builtins.find(_command);
    if(it != builtins.end()) {
        it->second(*this, fd_in, fd_out);
        return _pid = -1; // indicates a builtin
    }

    if ((pid = fork ()) == 0) {
        exec(fd_in, fd_out);
        exit(EXIT_FAILURE);
    }

    return _pid = pid;
}

/////////////////
// ProcessFlow //
/////////////////

// Add a process object
void ProcessFlow::add_process(Subprocess& process) {
    process._flow = this;
    _flow.push_back(process);
}

// Assign a file to read stdin from.
void ProcessFlow::set_input(const std::string& filename) {
    _input_filename = filename;
}

// Assign a file to write stdout to.
void ProcessFlow::set_output(const std::string& filename, bool append) {
    _output_filename = filename;
    _output_append = append;
}

// Opens files as a prepare stage.
bool ProcessFlow::prepare() {

    // Attach stdin to file if requested
    if (!_input_filename.empty()) {
        _input_handle = fopen(_input_filename.c_str(), "rb");

        if (_input_handle == NULL)
            throw(process_filenotfound(_input_filename));
    }

    // Attach stdout to file if requested
    if (!_output_filename.empty()) {
        _output_handle = fopen(_output_filename.c_str(),
                               _output_append ? "ab" : "wb");

        if (_output_handle == NULL)
            throw(process_filenotfound(_output_filename));
    }

    _ready = true;
    return _ready;
}

// Fork and exec
pid_t ProcessFlow::run(bool background) {
    if (_flow.empty()) return -1;

    int last_proc = _flow.size() - 1,
        fd_in  = STDIN_FILENO,
        fd_out = STDOUT_FILENO,
        pipes[2];

    for (int i=0 ; i <= last_proc ; i++) {
        Subprocess& process = _flow[i];

        // Create pipe, if needed
        if (i != last_proc && pipe(pipes) != 0)
            throw(process_exception("pipe creation failed"));

        if (i == 0) {
            // Attach stdin to file if requested
            if (_input_handle != NULL)
                fd_in = fileno(_input_handle);
            // Close stdin of first process if backgrounding and no redirect
            else if (background)
                fd_in = -1;
        }

        // Attach stdout to file if requested
        if (i == last_proc) {
            if (_output_handle != NULL)
                fd_out = fileno(_output_handle);
            else
                fd_out = STDOUT_FILENO;
        } else {
            fd_out = pipes[1];
        }

        process.run(fd_in, fd_out);

        if (i != last_proc) {
            fd_in = pipes[0]; // Set for the next loop
            close(pipes[1]);  // Close parent end, only used by child
        }

        // Close input/output files if we opened them
        if (i == 0 && _input_handle != NULL) {
            fclose(_input_handle);
            _input_handle = NULL;
        }

        if (i == last_proc && _output_handle != NULL) {
            fclose(_output_handle);
            _output_handle = NULL;
        }
    }

    // Return the last process in the chain
    if (background)
        return _flow.back()._pid;

    int saved_stdin = dup(STDIN_FILENO);
    close(STDIN_FILENO);
    int status;
    pid_t wait_pid = _flow.back()._pid;
    if (wait_pid > 0) 
        waitpid(wait_pid, &status, 0);
    dup2(saved_stdin, STDIN_FILENO);
}

void ProcessFlow::cleanup() {
    // None of these check errno for failure, on purpose
    if(_input_handle != NULL) {
        fclose(_input_handle);
        _input_handle = NULL;
    }

    if(_output_handle != NULL) {
        fclose(_output_handle);
        _output_handle = NULL;
    }
}

// Destructor, to flush and close file handles and pipes
ProcessFlow::~ProcessFlow() {
    cleanup();
}

///////////////////////
// ProcessController //
///////////////////////

bool _special_token(const std::string& token) {
    static const std::set<std::string> tokens {"&", ";", "|", "<", ">", ">>"};
    return tokens.count(token) > 0;
}

void ProcessController::update_child(pid_t pid, int status) {
}

void ProcessController::parse(const std::vector<std::string>& tokens) {
    int num_tokens = tokens.size(),
        last_index = num_tokens - 1;

    Subprocess current_cmd;
    std::shared_ptr<ProcessFlow> current_flow = std::make_shared<ProcessFlow>();
    bool current_special;

    for (int i=0; i < num_tokens; i++) {

        current_special = _special_token(tokens[i]);
        current_cmd._tokens.push_back(tokens[i]);

        // Handle error conditions
        if(current_special) {
            // Special token on empty command
            if(current_cmd._command.empty())
                throw(process_badtoken(tokens[i]));

            // Last is special but not & or ;
            else if (i == last_index && !(tokens[i] == "&" || tokens[i] == ";"))
                throw(process_badtoken("<end>"));

            // Two specials in a row
            else if (i < last_index && _special_token(tokens[i+1]))
                throw(process_badtoken(tokens[i+1]));
        }

        // Run as background job
        if (tokens[i] == "&") {
            current_flow->add_process(current_cmd);
            enqueue_job(current_flow, true);

            current_cmd = Subprocess();
            current_flow = std::make_shared<ProcessFlow>();
        }
        // Run as foreground job
        else if (tokens[i] == ";") {
            current_flow->add_process(current_cmd);
            enqueue_job(current_flow, false);

            current_cmd = Subprocess();
            current_flow = std::make_shared<ProcessFlow>();
        }
        // Pipe to next command
        else if (tokens[i] == "|") {
            current_flow->add_process(current_cmd);
            current_cmd = Subprocess();
        }
        // Overwrite file with stdout
        else if (tokens[i] == ">") {
            current_flow->set_output(tokens[++i]);
            current_cmd._tokens.push_back(tokens[i]);
        }
        // Append stdout to file
        else if (tokens[i] == ">>") {
            current_flow->set_output(tokens[++i], true);
            current_cmd._tokens.push_back(tokens[i]);
        }
        // Read stdin from file
        else if (tokens[i] == "<") {  
            current_flow->set_input(tokens[++i]);
            current_cmd._tokens.push_back(tokens[i]);
        }
        // Build command w/ args
        else {
            if(current_cmd._command.empty())
                current_cmd._command = tokens[i];
            else
                current_cmd._args.push_back(tokens[i]);
        }
    }

    if (!current_cmd._command.empty())
        current_flow->add_process(current_cmd);
    if (!current_flow->_flow.empty())
        enqueue_job(current_flow);
}

void ProcessController::run() {
    try {
        for(auto &flow_bg : _pending) {
            flow_bg.first->prepare();
        }
    } catch (std::exception& e) {
        reset_pending();
        throw;
    }

    // Populate and start background and foreground jobs, in order
    while (!_pending.empty()) {
        std::pair<std::shared_ptr<ProcessFlow>, bool> front = _pending.front();
        std::shared_ptr<ProcessFlow> flow = front.first;
        bool background = front.second;

        if(background) {
            _jobs.push_back(flow);
            _jobs.back()->run(true);
        } else {
            flow->run();
        }

        _pending.pop_front();
    }
}

void ProcessController::enqueue_job(std::shared_ptr<ProcessFlow> flow, bool background) {
    flow->_controller = this;
    _pending.emplace_back(flow, background);
}

// Retreive a job
std::shared_ptr<ProcessFlow> ProcessController::get_job(int job_id) const {
    if (_jobs.size() < job_id)
        throw(process_jobnotfound(job_id));
    return _jobs[job_id - 1];
}

// Searches jobs for a PID. If it's a background PID, return true.
const bool ProcessController::is_background_pid(int pid) const {
    for (const auto &job : _jobs)
        for (const Subprocess& process : job->_flow)
            if (process._pid == pid) return true;
    return false;
}
