cmake_minimum_required(VERSION 3.9)
project(calebj_PA1)

set(CMAKE_CXX_STANDARD 11)

add_executable(memtest memtest.cpp ackerman.cpp my_allocator.cpp)
