#ifndef BUILTINS_H
#define BUILTINS_H

#include <map>
#include <string>
#include <functional>

#include "process.hpp"

using BuiltinFunction = std::function<int(Subprocess&, int, int)>;

extern std::map<std::string, BuiltinFunction> builtins;

#endif
