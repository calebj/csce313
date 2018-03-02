#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include "tokenizer.hpp"

const std::string::size_type
Tokenizer::find_first_non_escaped(const std::string& str,
                                  const std::string& pat,
                                  const std::string::size_type& start
                                 ) const {
    std::string::size_type pos = start;
    int escape_balance;

    for (;;) {
        // Reset balance count
        escape_balance = 0;

        // Find next occurence of search, return npos if not found
        pos = str.find(pat, pos);
        if (pos == std::string::npos) return pos;

        // Count escapes, if any. If count is odd, next character is escaped.
        while (escape_balance < pos) {
            // If character before current position isn't an escape
            if (str.at(pos - escape_balance - 1) != _escape)
                break;
            else
                escape_balance++;
        }

        // If escapes are balanced, return match.
        if (escape_balance % 2 == 0)
            return pos;
        else
            pos += pat.length();
    }
}

const bool Tokenizer::starts_at(const std::string& string,
                                const std::string& test,
                                const std::string::size_type& position) {
    return std::equal(test.begin(), test.end(),
                      string.begin() + position);
}

const char* Tokenizer::which_starts(const std::string& string,
                                    const std::string& tests,
                                    const std::string::size_type& pos) {
    for (int i = 0; i < tests.length(); i++) {
        if (string.at(pos) == tests[i]) {
            return &tests[i];
        }
    }

    return NULL;
}

const std::string* Tokenizer::which_starts(const std::string& string,
                                           const vecstr& tests,
                                           const std::string::size_type& pos) {
    for (int i = 0; i < tests.size(); i++) {
        if (starts_at(string, tests[i], pos)) {
            return &tests.at(i);
        }
    }

    return NULL;
}

const pairstr* Tokenizer::which_starts(const std::string& string,
                                       const std::vector<pairstr>& tests,
                                       const std::string::size_type& pos) {
    for (int i = 0; i < tests.size(); i++) {
        if (starts_at(string, tests[i].first, pos)) {
            return &tests.at(i);
        }
    }

    return NULL;
}

const std::string Tokenizer::unescape_all(const std::string& string,
                                          const std::string& to_unescape) const {
    std::stringstream ret;

    std::string::size_type pos = string.find(_escape),
                           lastpos = 0,
                           slen = string.length(),
                           ulen = to_unescape.length();

    if (pos)
        ret << string.substr(0, pos);

    while (pos != std::string::npos) {
        if (starts_at(string, to_unescape, pos + 1)) {
            ret << to_unescape;
            pos += ulen + 1;
        } else if (pos + 1 < slen && string.at(pos + 1) == '\\') {
            ret << '\\';
            pos += 2;
        } else {
            ret << '\\';
            pos += 1;
        }

        lastpos = pos;
        pos = string.find(_escape, pos);
        ret << string.substr(lastpos, pos - lastpos);
    }

    return ret.str();
}

const void Tokenizer::tokenize(const std::string& input,
                               TokenizerResult& state) const {
    std::string::size_type pos  = 0, // Position in the input string
                           pos2 = 0, // temp var

                           len = input.length(); // Length of input

    bool complete = false,          // Is the current token complete?

         escape_next = false;       // Escape the next token?

    std::string tmpstr;             // A scratch string

    const std::string *strptr;      // A pointer for which_starts to set
    const pairstr *pstrptr;         // Same, but for which_starts on pairs

    // Reset state based on previous call. If we're being called from
    // a trailing escaped newline, the next one should be okay no matter
    // what. If we needed a quote, add a newline, since it means we were
    // called after the user pressed return without matching their quote.
    if (state._open_type == NEEDS_NEWLINE)
        state._open_type = TOKEN_OKAY;
    else if (state._open_type == NEEDS_QUOTE)
        state._token += '\n';

    while (len > pos) {
        // Handle closing quotes (or other end markers)
        if (!state._end.empty()) {
            // Look for a closing mark
            pos2 = find_first_non_escaped(input, state._end, pos);

            // Add text between here and close (or end) to current token
            if(pos2 == std::string::npos)
                // From pos to the end
                tmpstr = input.substr(pos, pos2);
            else
                // From pos to pos2 (doesn't include the closer!)
                tmpstr = input.substr(pos, pos2 - pos);

            tmpstr = unescape_all(tmpstr, state._end);

            // Append processed string to current token
            state._token += tmpstr;

            // If token is incomplete, return
            if (pos2 == std::string::npos) {
                return;
            } else {
                pos = pos2 + state._end.length();
                state._end.clear();
                state._open_type = TOKEN_OKAY;
            }
        }

        // Handle escaped characters and/or tokens
        else if (input[pos] == _escape) {
            pos++;

            // double escape = single character
            if (escape_next) {
                state._token += '\\';
            }

            // If escape is last character, append newline to token
            // and return state with NEEDS_NEWLINE set.
            else if (pos == len) {
                state._open_type = NEEDS_NEWLINE;
                state._token += '\n';
                return;
            }

            // Otherwise set the escape_next flag and restart the loop
            escape_next = true;
            continue;
        }

        // Special tokens that split regardless of whitespace unless escaped
        else if ((strptr = which_starts(input, _breaks, pos)) && strptr != NULL) {
            // In both cases, we do the same thing: append the token
            // to the string being built and advance pos. If this token was
            // escaped, maybe the next one is... so, not complete. But if the
            // pending token *was* empty, we know this one is complete.
            if (escape_next || state._token.empty()) {
                state._token += *strptr;
                pos += strptr->length();
                complete = !escape_next;
            } else {
                // Mark the current token complete and *don't* advance pos.
                // This token will be found again, and processed seperately.
                complete = true;
            }
        }

        // Handle opening quotes
        else if ((strptr = which_starts(input, _quotes, pos)) && strptr != NULL) {
            pos += strptr->length();

            if (escape_next) {
                state._token += *strptr;
            } else {
                state._end = *strptr;
                state._open_type = NEEDS_QUOTE;
            }
        }

        // Handle opening subshell
        else if ((pstrptr = which_starts(input, _subshell, pos)) && pstrptr != NULL) {
            pos += pstrptr->first.length();

            if (escape_next) {
                state._token += pstrptr->first;
            } else {
                state._end = pstrptr->second;
                state._open_type = NEEDS_SUBSHELL;
            }
        }

        // Handle everything else
        else if (!escape_next) {
            // Skip whitespace
            if (which_starts(input, _spaces, pos) != NULL) {
                pos++;

                // Whitespace marks the end of a token
                complete = !state._token.empty();
            }

            // Return when we hit a comment to skip everything after it
            else if (which_starts(input, _comments, pos) != NULL)
                return;

            // Default to adding individual characters
            else {
                state._token += input[pos++];
            }
        }

        // Escaped characters get added too, but even non-special ones don't
        // have their escape character included.
        else {
            state._token += input[pos++];
        }

        if (complete) {
            complete = false;
            state.tokens.push_back(state._token);
            state._token.clear();
            state._open_type = TOKEN_OKAY;
        }

        escape_next = false;
    }

    if (!state._token.empty() && state._open_type == TOKEN_OKAY) {
        state.tokens.push_back(state._token);
        state._token.clear();
    }
}

const TokenizerResult Tokenizer::tokenize(const std::string& input) const {
    TokenizerResult state;
    tokenize(input, state);
    return state;
}

void TokenizerResult::reset() {
    tokens.clear();
    _token.clear();
    _end.clear();
    _open_type = TOKEN_OKAY;
}
