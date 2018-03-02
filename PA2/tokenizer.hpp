#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <vector>
#include <string_view>
#include <utility>

// Shorthands
using vecstr = std::vector<std::string>;
using pairstr = std::pair<std::string, std::string>;

// Used to indicate what has been left incomplete/unclosed
enum OpenType {
    TOKEN_OKAY,     // No further input required
    NEEDS_QUOTE,    // User left a quote open
    NEEDS_SUBSHELL, // Missing backtick or close-paren
    NEEDS_NEWLINE,  // User left escape as the last character
    NEEDS_HEREDOC   // Waiting for heredoc marker
};

enum TokenType {
    STRING,
    QUOTED,
    SUBSHELL
};

class Token {
    std::string str;
    TokenType type;
};

// This class is returned by tokenize() in order to allow continued
// parsing in the event that a quote or heredoc is active when the
// user submits a line.
class TokenizerResult {

    // So Tokenizer member functions can set attributes directly
    friend class Tokenizer;

public:
    // Vector of parsed tokens
    vecstr tokens {};

    // Indicates whether we need more input.
    const OpenType get_open_type() const { return _open_type; };

    // Resets the state of the result to empty
    void reset();

// private:
    OpenType _open_type = TOKEN_OKAY;

    std::string _token, // The token currently being built
                _end;   // The closing token being looked for
};

class Tokenizer {
public:
    // Called on an input string 
    const TokenizerResult tokenize(const std::string& input) const;

    // Mutates an existing TokenizerResult
    const void tokenize(const std::string& input,
                        TokenizerResult& base) const;

    const std::string::size_type
        find_first_non_escaped(const std::string& str,
                               const std::string& pat,
                               const std::string::size_type& start = 0
                              ) const;

    const std::string unescape_all(const std::string& string,
                                   const std::string& to_unescape) const;

    static const bool starts_at(const std::string& string,
                                const std::string& test,
                                const std::string::size_type& pos = 0);

    // These search for any element in tests that starts at/equals string[pos]
    // and return a pointer to it. If none match, they return NULL.
    static const char* which_starts(const std::string& string,
                                    const std::string& tests,
                                    const std::string::size_type& pos = 0);

    static const std::string* which_starts(const std::string& string,
                                           const vecstr& tests,
                                           const std::string::size_type& pos = 0);

    static const pairstr* which_starts(const std::string& string,
                                       const std::vector<pairstr>& tests,
                                       const std::string::size_type& pos = 0);

    void set_escape   (const char& newescape) { _escape = newescape; }

//  void set_heredoc  (const std::string& newheredoc) { _heredoc = newheredoc; }

    void set_quotes   (const vecstr& newquotes)   { _quotes = newquotes; }
    void set_spaces   (const vecstr& newspaces)   { _spaces = newspaces; }
//  void set_delims   (const vecstr& newdelims)   { _delims = newdelims; }
    void set_comments (const vecstr& newcomments) { _comments = newcomments; }
    void set_breaks   (const vecstr& newbreaks)   { _breaks = newbreaks; }

private:
    char _escape = '\\';          // Ignores any special meaning of next token

//  std::string _heredoc = "<<";  // Read everything that follows as a single
                                  // token, including newlines, until the
                                  // token after this is encountered alone

    vecstr _quotes  {"\"", "\'"}, // Merges contents into one token

           _spaces  {" ", "\t",   // Defines characters which split tokens
                     "\r", "\n", "\r\n"},

//         _delims {","},         // Splits tokens but are ignored

           _comments = {"#"},     // Ignores until newline

           _breaks  {"|", ";",    // Pipe and sequence
                     "&",         // Sequence and 
                     ">>", ">",   // - stdout append and overwrite to file
                     "<"};        // - read from file

    std::vector<pairstr> _subshell = { // Opening and closing subshell marks
        {"$(", ")"},                   // - $(...)
        {"`", "`"}                     // - `...`
    };
};

#endif
