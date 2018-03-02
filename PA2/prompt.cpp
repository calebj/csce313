#include <signal.h> // SIGINT, sigaction, struct sigaction, sigemptyset
#include <unistd.h> // get_current_dir_name()
#include <libgen.h> // basename()
#include <stdlib.h> // getenv()
#include <time.h>   // time_t, struct tm, time(), localtime(), strftime()

#include <linux/limits.h>   // PATH_MAX
#include <bits/local_lim.h> // HOST_NAME_MAX

#include <stdio.h>  // FILE, since readline.h doesn't include it >_>
#include <readline/readline.h> // readline, duh
#include <readline/history.h>  // readline history code

#include <string>
#include <iostream>
#include <sstream>

#include "prompt.hpp"

sigjmp_buf _sigint_buf;

const std::string Prompt::DEFAULT_PS1 = "[\\u@\\H \\W]\\$ ";
const std::string Prompt::DEFAULT_PS2 = "> ";

void _int_handler(int status) {
    // Handle ctrl-c by clearing and redisplaying the prompt on a new line
    siglongjmp(_sigint_buf, 1);
}

const std::string Prompt::do_prompt(const std::string& prompt, bool return_empty) {
    // Call format_ps to format the provided prompt spec
    std::string _prompt = parse_ps(prompt);

    // Set up space to swap handlers
    struct sigaction newHandler;
    struct sigaction oldHandler;

    // I've just been in this place before!
    bool dejavu = false;

    // Clear new handler flags and set it to use the handler above
    memset(&newHandler, 0, sizeof(newHandler));
    sigemptyset(&newHandler.sa_mask);
    newHandler.sa_handler = _int_handler;

    while ( sigsetjmp(_sigint_buf, 1) != 0 );

    // And I know it's my time to come home! CALLING YOU!
    if (dejavu) {
        // Restore old handler
        sigaction(SIGINT, &oldHandler, NULL);

        // Raise exception
        throw(prompt_sigint());
    }

    // AND THE SUBJECT'S A MYSTERY!
    dejavu = true;

    // Infinite loop until nonempty, non-^C is entered
    for (;;) {
        // Register SIGINT (ctrl-c) handler
        sigaction(SIGINT, &newHandler, &oldHandler);

        // Invoke readline to prompt user for a command
        buf = readline(_prompt.c_str());

        // Restore old signal handler
        sigaction(SIGINT, &oldHandler, NULL);

        if (!buf) {
            throw(prompt_eof()); // throw EOF, handled in main
        } else if (strcmp(buf, "") != 0) {
            break;               // input is nonempty
        } else if (return_empty) {
            free(buf);           // Free readline's buffer
            return "\n";         // return newline
        }
    }

    // Add the command to readline's history
    add_history(buf);

    // Read the command into a string
    std::string ret;
    ret.assign((const char*) buf);

    // Free the c string allocated by readline, then return the c++ one
    free(buf);
    return ret;
}

const std::string Prompt::do_prompt(bool return_empty) {
    // when called without an argument, fall back to default
    return do_prompt(DEFAULT_PS1, return_empty);
}

const std::string Prompt::parse_ps(const std::string& spec) {
    std::string::size_type spec_len = spec.length();
    std::stringstream ss;
    std::string elem, special;

    for ( std::string::size_type i=0; i < spec_len; i++ ) {
        if (spec.at(i) == '\\') {
            special = spec.substr(i, 2);
            elem = _ps_element(special);
            i++; // skip two
        } else {
            elem = spec.at(i);
        }

        ss << elem;
    }

    std::string ret = ss.str();
    return ret;
}

const std::string Prompt::_ps_element(const std::string& elem) {
    std::string ret;
    char current_path[PATH_MAX],
         hostname[HOST_NAME_MAX],
         timestr_buf[64];
    time_t rawtime;
    struct tm* timeinfo;

    // Read current path, hostname and time
    getcwd(current_path, PATH_MAX);
    gethostname(hostname, HOST_NAME_MAX);
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // normal characters such as newline, backslash, CR and BEL
    if (elem == "\\n") return "\n";
    else if (elem == "\\\\") return "\\";
    else if (elem == "\\r") return "\r";
    else if (elem == "\\a") return "\a";

    // Current directory
    else if (elem == "\\w") { // full current path
        ret.assign((const char*)current_path);
        return ret;
    } else if (elem == "\\W") { // basename of current path
        char* base_path = basename(current_path);
        ret.assign((const char*)base_path);
        return ret;
    }

    // Current user and user/root prompt
    else if (elem == "\\u") {
        ret.assign((const char*)getenv("USER"));
        return ret;
    } else if (elem == "\\$") { // Return # if root, else $
        if (geteuid()) // true if nonzero
            return "$";
        else
            return "#";
    }

    // Date and time
    else if (elem == "\\t") { // HH:MM:SS (24h)
        strftime(timestr_buf, 64, "%H:%M:%S", timeinfo);
        ret.assign((const char*)timestr_buf);
        return ret;
    } else if (elem == "\\T") { // HH:MM:SS (12h)
        strftime(timestr_buf, 64, "%I:%M:%S", timeinfo);
        ret.assign((const char*)timestr_buf);
        return ret;
    } else if (elem == "\\@") { // HH:MM AM/PM
        strftime(timestr_buf, 64, "%I:%M %p", timeinfo);
        ret.assign((const char*)timestr_buf);
        return ret;
    } else if (elem == "\\A") { // HH:MM (24h)
        strftime(timestr_buf, 64, "%H:%M", timeinfo);
        ret.assign((const char*)timestr_buf);
        return ret;
    } else if (elem == "\\d") { // Weekday Month Day (e.g. Fri Aug 03)
        strftime(timestr_buf, 64, "%a %b %d", timeinfo);
        ret.assign((const char*)timestr_buf);
        return ret;
    }

    else if (elem == "\\H") {
        ret.assign(hostname);
        return ret;
    } else if (elem == "\\h") {
        ret.assign(hostname);

        // Erase everything after first dot, if any is found
        std::string::size_type first_dot = ret.find('.');
        if (first_dot != std::string::npos)
            ret.erase(first_dot, std::string::npos);

        return ret;
    }

    else { // Default/unknown
        return elem;
    }
}

void Prompt::display_prompt_help() {
    std::cout << "Prompt spec special components:\n"
              << "\\\\, \\r, \\n, \\a : backslash, CR, LF, BEL\n"
              << "\\w : Full CWD\n"
              << "\\W : Basename of CWD\n"
              << "\\u : Current username\n"
              << "\\$ : $ if UID != 0, else #\n"
              << "\\t : HH:MM:SS (24h)\n"
              << "\\T : HH:MM:SS (12h)\n"
              << "\\@ : HH:MM AM/PM\n"
              << "\\A : HH:MM (24h)\n"
              << "\\d : Weekday Month Day (e.g. Fri Aug 03)\n"
              << "\\H : Full hostname\n"
              << "\\h : Hostname without domain\n";
}
