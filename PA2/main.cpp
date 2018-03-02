#include <iostream>
#include <string>
#include <sstream>
#include <exception>
#include <cstring>
#include <map>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "prompt.hpp"
#include "tokenizer.hpp"
#include "process.hpp"

void print_usage(char **argv) {
    std::cout << "Johnson Shell\n\n"
              << "Usage:\n " << argv[0] << " [-h] [-t] [-p <PS1>] [-q <PS2>]"
              << "\n\nOptions:\n"
              << " -h        Prints this help and exits.\n"
              << " -t        Disables display of any prompts.\n"
              << " -p <PS1>  Sets prompt spec 1. Use HELP for options.\n"
              << "           Defaults to " << Prompt::DEFAULT_PS1 << "\n"
              << " -q <PS2>  Sets prompt spec 2. Used for e.g. open quotes.\n"
              << "           Defaults to " << Prompt::DEFAULT_PS2 << "\n";
}

// Holding buffer for messages to display before prompting again
std::map<pid_t, int> notifications;

// Handler for SIGCHLD
static void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    while (pid = waitpid(-1, &status, WNOHANG), pid > 0)
        notifications[pid] = status;
}

void display_notifications(ProcessController& controller) {
    // Print any pending messages about background processes
    for (auto &kv : notifications) {
        // If notification was about a non-backgrounded process,
        // skip it.
        int pid = kv.first,
        status = kv.second;
        notifications.erase(pid);

        controller.update_child(pid, status);

        if (!controller.is_background_pid(pid))
            continue;

        std::cout << "Child process ID " << pid;

        if (WIFEXITED(status)) {
            std::cout << " exited, status=" << WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            std::cout << " killed by signal " << WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            std::cout << " stopped by signal " << WSTOPSIG(status);
        } else if (WIFCONTINUED(status)) {
            std::cout << " continued";
        } else {
            std::cout << "unknown signal " << status;
        }

        std::cout << std::endl;
    }
}

int main(int argc, char **argv) {
    Prompt prompt;
    Tokenizer tokenizer;
    TokenizerResult state;
    ProcessController controller;

    bool null_prompt = false; // Should we display a prompt? (-t flag)

    // Used for argument parsing
    int index, c;
    opterr = 0;
    errno = 0;

    std::string input_line,
                normal_ps = Prompt::DEFAULT_PS1,
                continue_ps = Prompt::DEFAULT_PS2,
                prompt_spec;

    // Process arguments
    while ((c = getopt(argc, argv, "htp:q:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv);
                exit(EXIT_SUCCESS);
            case 't':
                null_prompt = true;
                break;
            case 'p':
                if (!strcmp(optarg, "HELP")) {
                    Prompt::display_prompt_help();
                    exit(EXIT_SUCCESS);
                }
                normal_ps.assign((const char*) optarg);
                break;
            case 'q':
                if (!strcmp(optarg, "HELP")) {
                    Prompt::display_prompt_help();
                    exit(EXIT_SUCCESS);
                }
                continue_ps.assign((const char*) optarg);
                break;
            case '?':
                if (optopt == 'p' || optopt == 'q')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }


    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigchld_handler;

    if (sigaction(SIGCHLD, &act, 0)) {
        perror("sigaction");
        return 1;
    }

    prompt_spec = normal_ps;

    for (;;) {
        try {
            input_line = prompt.do_prompt(null_prompt ? "" : prompt_spec,
                                          true);

            if ((state.get_open_type() == TOKEN_OKAY) && input_line == "\n") {
                display_notifications(controller);
                continue;
            }

            tokenizer.tokenize(input_line, state);

            if (state.get_open_type() == TOKEN_OKAY) {
                prompt_spec = normal_ps;

                controller.parse(state.tokens);
                controller.run();
                display_notifications(controller);

                state.reset();


            } else {
                prompt_spec = continue_ps;
            }

        } catch (prompt_sigint& e) {
            // Print a newline and reset state on ^C
            std::cout << std::endl;
            prompt_spec = normal_ps;
            state.reset();
        } catch (prompt_eof& e) {
            // Exit with OK status on EOF/^D
            std::cout << std::endl;
            return 0;
        } catch (process_exception& e) {
            std::cout << e.what() << std::endl;
            state.reset();
        } catch (std::exception& e) {
            // Catch and print other exceptions, then exit
            std::cout << e.what() << std::endl;
            return 1;
        }
    }
    return 0;
}
