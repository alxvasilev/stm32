cmake_minimum_required(VERSION 2.8)
include_directories(../../include)
add_definitions(-std=c++14 --sanitize=address -DSTM32PP_NOT_EMBEDDED)
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} --sanitize=address)
message(STATUS "${STM32PP_SRCS}")
add_executable(tprintf-test ../../src/tsnprintf.cpp ../../src/tprintf.cpp main.cpp)
