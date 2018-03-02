cmake_minimum_required(VERSION 3.9)
project(calebj_PA2)

set(CMAKE_CXX_STANDARD 11)

add_executable(jsh main.cpp builtins.cpp process.cpp prompt.cpp tokenizer.cpp)
target_link_libraries(jsh readline)
