cmake_minimum_required(VERSION 3.9)
project(calebj_PA3)

set(CMAKE_CXX_STANDARD 11)

add_library(reqchannel reqchannel.cpp)

add_executable(client client.cpp atomic_stdout.hpp)
target_link_libraries(client pthread rt reqchannel)

add_executable(dataserver dataserver.cpp)
target_link_libraries(dataserver pthread rt reqchannel)
