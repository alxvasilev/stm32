cmake_minimum_required(VERSION 2.8)
include_directories(../../include)
add_definitions(-std=c++14)
add_executable(nvstore-test flash.cpp)
