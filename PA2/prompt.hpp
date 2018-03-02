#ifndef PROMPT_H
#define PROMPT_H

#include <setjmp.h> // sigjmp_buf

#include <string>
#include <exception>

class Prompt {
public:
    // Default prompt specs (defined in prompt.cpp)
    static const std::string DEFAULT_PS1, DEFAULT_PS2;

    static void display_prompt_help();

    // Read user input
    const std::string do_prompt(bool return_empty = false);
    const std::string do_prompt(const std::string& prompt,
                                bool return_empty = false);

    // Parse prompt spec
    const std::string parse_ps(const std::string& spec);

private:
    char* buf;
    const std::string _ps_element(const std::string& elem);
};

class prompt_eof : public std::exception {};
class prompt_sigint : public std::exception {};

void _int_handler(int status);

#endif
