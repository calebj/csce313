cmake_minimum_required(VERSION 3.9)
project(calebj_PA6)

set(CMAKE_CXX_STANDARD 11)

add_library(reqchannel reqchannel.cpp)

add_executable(client client.cpp bounded_buffer.cpp)
target_link_libraries(client pthread reqchannel rt)

add_executable(dataserver dataserver.cpp)
target_link_libraries(dataserver pthread reqchannel rt)