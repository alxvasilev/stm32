cmake_minimum_required(VERSION 3.0)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSROOT "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-as)
set(CMAKE_LINKER arm-none-eabi-ld)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -fno-exceptions -fno-rtti -mcpu=cortex-m3 -mthumb -nostartfiles" CACHE STRING "")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}" CACHE STRING "")
set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "")

if (optUseOpencm3)
    set(optUseOpencm3 1 CACHE BOOL "Use the libopencm3 platform" FORCE)
    set(optChipFamily STM32F1 CACHE STRING "Chip family for opencm3" FORCE)
    set_property(CACHE optChipFamily PROPERTY STRINGS STM32F0 STM32F1 STM32F2
        STM32F3 STM32F4 STM32F7 STM32L0 STM32L1 STM32L4)
    string(SUBSTRING "${optChipFamily}" 5 2 stm32model_uc)
    string(TOLOWER "${stm32model_uc}" stm32model)

    set(ldscriptBaseDir "${CMAKE_CURRENT_LIST_DIR}/libopencm3/lib/stm32")
    set(modeldir "${ldscriptBaseDir}/${stm32model}")
    file(GLOB ldscripts "${modeldir}/stm32*.ld")
    set(optLinkScript "${ldscriptBaseDir}/f1/stm32f103xb.ld" CACHE STRING "" FORCE)
    set_property(CACHE optLinkScript PROPERTY STRINGS ${ldscripts})
    link_directories("${CMAKE_CURRENT_LIST_DIR}/libopencm3/lib")
    link_libraries("opencm3_stm32${stm32model}")
    include_directories("${CMAKE_CURRENT_LIST_DIR}/libopencm3/include")
else()
    set(optUseOpencm3 0 CACHE BOOL "Use the libopencm3 platform" FORCE)
    set(defaultLinkScript "${CMAKE_SYSROOT}/stm32.ld")
    set(optChipFamily STM32F10X_MD CACHE STRING "Chip family for platform headers" FORCE)
    set_property(CACHE optChipFamily PROPERTY STRINGS STM32F10X_LD STM32F10X_LD_VL
        STM32F10X_MD STM32F10X_MD_VL STM32F10X_HD STM32F10X_HD_VL STM32F10X_XL STM32F10X_CL)
    set(optLinkScript "${defaultLinkScript}" CACHE PATH "Linker script" FORCE)
endif()

add_definitions(-D${optChipFamily} -Wall)
include_directories("${CMAKE_CURRENT_LIST_DIR}/stm32++/include")
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -T${optLinkScript}" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "--specs=nosys.specs")

if (optNoSemihosting)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "--specs=nosys.specs" CACHE STRING "")
else()
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "--specs=rdimon.specs -lc" CACHE STRING "")
endif()

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_Property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)

