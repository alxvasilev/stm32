# stm32-env

This project aims to create a more or less complete STM32 development environment,
based on gcc-none-eabi, OpenOCD for interfacing with the controller,
CMake for building, optionally libopencm3 for hardware abstraction. On top of
the libopencm3 hardware abstraction, the stm32++ C++ library is provided,
which harnesses the power of C++11 to implement a simple, hardware-abstracted
programming API for STM32 development. It makes heavy use of C++ templates in
order to move as much programming logic and state as possible to the compile stage,
rather than runtime. This results in faster code and less RAM usage. stm32++
also includes drivers for some popular hardware such as displays and sensors.
The library is located in the stm32++ subdirectory and has a dedicated Readme file.
Please have in mind that the library is a work in progress, it is not perfect and
there may be bugs.

Scripts are provided for:
 - setting up the shell environment: `env-stm32.sh`
 - executing OpenOCD commands and starting it if it's not running: `ocmd.sh`
 - flash an image, using ocmd.sh: `flash.sh`

Additionally, the environment can work in a minimalistic fashion, without
any base libraries, but just `STM32F10x.h`, a minimal set of CMSIS headers and a minimal
link script (`stm32.ld`). This mode can be suitable for writing bootloaders.
Each component is described in more detail below:

## env-stm32.sh

It must be _sourced_ (and not run in a subshell). What it does is to provide a
 thin shell around the cmake command that passes to it the `stm32-toolchain.cmake`
 toolchain. The actual work of setting up the build is done by the toolchain file.
 The env script also sets up convenience aliases for the gcc toolchain, so that for
 example gcc maps to arm-none-eabi-gcc, etc. It also alters the shell propmt to easily
 identify shells that have the environment setup.
 After sourcing this script in the shell, instead of running the command `cmake`,
 you should run `xcmake` with any options you would normally pass to cmake.
 This automatically picks up the STM32 toolhain.
 IMPORTANT: If you want to setup the project to use libopencm3, the option -DoptUseOpencm3
 *must* be provided at the initial run of xcmake. If it is not, modifying it later
 currently causes inconsistency of the build flags and the build will not work
 properly. If you need to change that option, currently you need to delete the
 cmake cache and re-configure the project from scratch.
 The env script also provides convenience aliases for:
  - the toolchain executables, removing the necessity to type the arm-none-eabi- prefix.
    That is, gdb will map to arm-none-eabi-gdb, gcc will map to arm-none-eabi-gcc, etc
  - the `ocmd.sh` and `flash.sh` scripts, so they can be invoked as shell functions rather
    than specifying path and filename. That is, instead of running
 
    `/path/to/ocmd.sh <command(s)>`
    you can run:
    `ocmd <command(s)>`
 
## stm32-toolchain.cmake
This is the cmake toolchain that will be used by CMake. It:

- Sets up the CMake sysroot for the toolchain to point to the appropriate location.
The CMake sysroot directory is intentended to contain system headers and libraries,
and is the default search location when CMake is instructed to use a library.
On Unix-like systems, the sysroot is usually the /usr directory, where system-wide
libs and headers are installed.
    Two versions of the sysroot directory are provided:
    - `./sysroot` - when libopencm3 and stm32++
    - `./sysroot-baremetal` - when not using any libraries, programming baremetal.

- Based on the option to use or not libopencm3 (`optUseOpenCm3`), and on the type
 of chip used (`optChipFamily`), sets up a global define for the chip type and
 path to a linker script. The toolchain script automatically detects the supported
 by libopencm3 STM32 families(F1, F2, ...) and, based on the selected one, detects
 the supported models and fills up the possible linker script paths. 
 For the barebones mode (no libopencm3), stm32.ld is used, which may be too
 minimalistic and the user may want to copy and extend it. In barebones mode, the
 script path can be manually set. This is unlike the opencm3 mode, where the linker
 script path is a drop down list with the libopencm3-provided ones, for the selected
 chip family. You should choose the one that matches the exact model of your chip.

- `stdio`
If `optStdioLibcInDebug` or `optStdioLibcInRelease` are enabled, the linker
 is directed to use a version of the standard C library that has an implementation
 of stdio, with semihosting support (`--specs=rdimon.specs -lc`). This enables the
 use of file descriptors, printf() and similar standard functions for standard I/O
 output. The file descriptors don't currespond to real files, but transparently direct
 the I/O streams to OpenOCD, which in turn can direct the I/O to console or files.
 This mechanism is called 'semihosting'. OpenOCD has to be configured to support this.
 The `ocmd.sh` script always enables that support. The controller side of the semihosting
 is very simple to implement - just with a few lines of assembly code that call the
 BKPT / SVC ARM instruction. It is not necessary to use the stdio-enabled C
 standard lib, if the only thing needed is simple console output for logging.
 Support for this is built into the stm32++ library, which provides a very fast and
 lightweight `printf`-like formatting facility - `tprintf`. It is implemented
 using C++ templates and is type-safe, unlike the classic printf. The argument type
 and format detection is static, at compile time, which greatly speeds up
 the parsing of the format string at runtime. See the documentation of the stm32++
 library for more details.
- CMake conveneice defines and functions, as follows:
  - `STM32PP_SRCS` - The .cpp source files of the stm32++ library. Add this to
  your application's source file list if you want to use stm32++
  - `stm32_create_utility_targets(imgname)` - Call this function in order to have
  the convenience `flash` and `gdb` make targets automatically created and set up.

## ocmd.sh
Sends a command to OpenOCD via the telnet port. First the script checks if 
 OpenOCD is running. If not, it is started and the script waits till it starts
 accepting telnet connections. Multiple commands can be sent in one call, but they
 must all be placed in quotes and separated by semicolons.
 To shut down OpenOCD you can send the `shutdown` command. Any call to ocmd.sh will
 then first bring up OpenOCD again and execute the command. For help on usage,
 you can call the scripts without arguments.

## flash.sh
Flashes the specified file to the chip and either resets or halts it, depending on
a command line option. For help on usage, you can call the script without arguments.

## Example session

```
// Install the environment
$ source ../../stm32-env/env-stm32.sh
// Create project directory
$ mkdir myproject
$ cd myproject
// Create CMakeLists.txt file that describes how to build the project
$ echo -e \
"cmake_minimum_required(VERSION 2.8)\n\
\n\
project(myproject)\n\
set(SRCS main.cpp ${STM32PP_SRCS})\n\
set(imgname "${CMAKE_PROJECT_NAME}.elf")\n\
add_executable(${imgname} ${SRCS})\n\
stm32_create_utility_targets(${imgname})\n\"\
> ./CMakeLists.txt

// Create a .cpp source file with the main() function
$ echo "int main() { for (;;); return 0; }" > ./main.cpp

// Create build directory
$ mkdir build
$ cd build

// Initialize cmake configuration
$ xcmake ..

// Edit cmake configuration, check and set options
$ ccmake .

// Build the application, flash it to the chip and run it.
// The ST-Link must be connected
$ make flash

// reset the chip
$ ocmd reset
```
