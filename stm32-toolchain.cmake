
cmake_policy(VERSION 3.4)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSROOT "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-as)
set(CMAKE_LINKER arm-none-eabi-ld)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -mcpu=cortex-m3 -mthumb -nostartfiles" CACHE STRING "")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}" CACHE STRING "")

set(defaultLinkScript "${CMAKE_CURRENT_SOURCE_DIR}/linkscript.ld")
set(optLinkScript "${defaultLinkScript}" CACHE PATH "Linker script")
set(optChipFamily STM32F10X_MD CACHE STRING "Chip family for platform headers")
set_property(CACHE optChipFamily PROPERTY STRINGS STM32F10X_LD STM32F10X_LD_VL
    STM32F10X_MD STM32F10X_MD_VL STM32F10X_HD STM32F10X_HD_VL STM32F10X_XL STM32F10X_CL)

if ((optLinkScript STREQUAL "${defaultLinkScript}") AND (NOT EXISTS "${defaultLinkScript}"))
    message(STATUS "Copying default linker script to souce dir...")
    FILE (COPY "${CMAKE_SYSROOT}/stm32.ld" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}")
    FILE (RENAME "${CMAKE_CURRENT_SOURCE_DIR}/stm32.ld" "${defaultLinkScript}")
endif()

add_definitions(-D${optChipFamily})
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -T${optLinkScript}" CACHE STRING "")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")


