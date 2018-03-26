#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <stack>

#include <stdio.h>
#include <string.h>       // strerror()
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <linux/limits.h> // PATH_MAX

#include "process.hpp"
#include "builtins.hpp"

int cd_function(Subprocess& proc, int fd_in, int fd_out) {
    const char *dir;
    char tmp_dir[PATH_MAX],
         cwd[PATH_MAX];
    const auto& args = proc.get_args();
    dirstack_t& dirstack = proc.get_flow().get_controller().get_dirstack();
    std::string target;

    // Handle arg count > 1
    if (args.size() > 1) {
        std::cerr << "cd: too many arguments!" << std::endl;
        return 1;
    }

    // Handle no arg or arg = ~
    else if (args.empty() || args[0] == "~") {
        if ((dir = getenv("HOME")) == NULL)
            dir = getpwuid(getuid())->pw_dir;
    }

    // Pop from dirstack
    else if (args[0] == "-") {
        if (dirstack.size() == 0) {
            std::cerr << "cd: directory stack is empty!" << std::endl;
            return 1;
        }
        strcpy(tmp_dir, dirstack.back().c_str());
        dir = tmp_dir;
        dirstack.pop_back();
    }

    // everything else
    else {
        strcpy(tmp_dir, args[0].c_str());
        dir = tmp_dir;
    }

    // Only push onto stack if we didn't pop from it
    if(args[0] != "-") {
        getcwd(cwd, PATH_MAX);
        dirstack.emplace_back(cwd);
    }

    if (chdir(dir)) {
        std::cerr << "cd: " << dir << ": " << strerror(errno) << std::endl;
        dirstack.pop_back();
        return 1;
    }

    return 0;
};

int dirs_function(Subprocess& proc, int fd_in, int fd_out) {
    std::stringstream ss;
    dirstack_t& dirstack = proc.get_flow().get_controller().get_dirstack();

    for(int i=dirstack.size()-1; i >= 0 ; i--) {
        ss << dirstack[i];

        if(i != 0)
            ss << ' ';
        else
            ss << '\n';
    }

    dprintf(fd_out, ss.str().c_str());
};

int pwd_function(Subprocess& proc, int fd_in, int fd_out) {
    char current_path[PATH_MAX];
    getcwd(current_path, PATH_MAX);
    dprintf(fd_out, "%s\n", current_path);
};

int jobs_function(Subprocess& proc, int fd_in, int fd_out) {
    ProcessController& controller = proc.get_flow().get_controller();
    const auto& jobs = controller.get_jobs();
    std::stringstream ss;

    for (int i = 0; i < jobs.size(); i++) {
        std::string state;

        ss << "Job " << i + 1 << " (" << state << "):" << std::endl;
        for (const auto& process : jobs[i]->get_flow()) {
            ss << " - " << process.get_pid() << " :";
            for(const auto& token : process.get_tokens())
                ss << " " << token;
            ss << std::endl;
        }
    }
    dprintf(fd_out, ss.str().c_str());
    return 0;
};

// int exec_function(Subprocess& proc, int fd_in, int fd_out) {
//     Subprocess newproc;
// };

std::map<std::string, BuiltinFunction> builtins = {
    {"cd",   &cd_function},
    {"dirs", &dirs_function},
    {"pwd",  &pwd_function},
    {"jobs", &jobs_function},
//     {"exec", &exec_function}
};
