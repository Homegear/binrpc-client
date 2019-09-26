cmake_minimum_required(VERSION 3.8)
project(binrpc)

set(CMAKE_CXX_STANDARD 17)

add_executable(binrpc main.cpp)

target_link_libraries(binrpc homegear-base gnutls gcrypt pthread)