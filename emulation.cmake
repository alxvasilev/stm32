# @author Alexander Vassilev
# @copyright BSD License

# CMake include file for building stm32++ projects as desktop applications,
# in emulation mode. It can be used to build, test and debug hardware-independent
# code, and provide emulation for hardware devices, such as an LCD display. For that
# purpose, the wxWidgets library is used to provide the GUI
# Usage:
# You need to have sourced the env_stm32.sh script, as usual
# In your CMakeLists.txt file, include this file, and you will have basic facilities
# similar to the ones provided by the arm toolchain file.
# The STM32PP_NOT_EMBEDDED define is provided, so that source code can detect
# when it is cumpiled under this emulation environment.
# Example CMakeLists.txt file:
#
# cmake_minimum_required(VERSION 2.8)
# project(lcdemu)
# include($ENV{STM32_ENV_DIR}/emulation.cmake)
# set(SRCS emu.cpp ${STM32PP_SRCS} ${STM32PP_SRCPATH}/stdfonts.cpp)
# set(imgname "${CMAKE_PROJECT_NAME}.elf")
# add_executable(${imgname} ${SRCS})
# stm32_create_utility_targets(${imgname})

cmake_minimum_required(VERSION 3.0)

find_package(wxWidgets COMPONENTS core base)
if (NOT wxWidgets_FOUND)
    message(STATUS "wxWidgets not found, GUI will not be available")
endif()

set(ENV_SCRIPTS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Default to debug build
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_Property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)

# Utilities to facilitate user CMakeLists
set(STM32PP_SRCPATH "${ENV_SCRIPTS_DIR}/stm32++/src")
set(STM32PP_SRCS
        "${STM32PP_SRCPATH}/printSink.cpp"
        "${STM32PP_SRCPATH}/tsnprintf.cpp"
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" CACHE STRING "")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -std=c++14 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics ${wxWidgets_CXX_FLAGS}" CACHE STRING "")

include_directories("${ENV_SCRIPTS_DIR}/stm32++/include" ${wxWidgets_INCLUDE_DIRS})
link_directories(${wxWidgets_LIBRARY_DIRS})
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS}" ${wxWidgets_DEFINITIONS})
set(CMAKE_CPP_FLAGS_DEBUG "${CMAKE_CPP_FLAGS}" ${wxWidgets_DEFINITIONS_DEBUG})
add_definitions(-DSTM32PP_NOT_EMBEDDED=1)
function(stm32_create_utility_targets imgname)
    target_link_libraries(${imgname} ${wxWidgets_LIBRARIES})
    add_custom_target(gdb
        gdb -ex 'file ${CMAKE_CURRENT_BINARY_DIR}/${imgname}'
        -ex 'directory ${CMAKE_CURRENT_SOURCE_DIR}'
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS "${imgname}")
endfunction()
