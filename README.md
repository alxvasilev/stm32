# stm32-misc

This project aims to create a more or less complete STM32 development environment,
based on gcc-none-eabi, OpenOCD for interfacing with the controller,
CMake for building, and optionally libopencm3 for hardware abstraction.
On top of libopencm3, optional convenience C++ classes are provided (in the stm32++
subdir), wich aim to facilitate high level C++ object-oriented design, but with no
(or in some cases minimal) overhead.

Scripts are provided for:
 - setting up the shell environment: env-stm32.sh
 - executing OpenOCD commands and starting it if it's not running: ocmd.sh
 - flash an image, using ocmd.sh: flash.sh

Additionally, the environment can work in a minimalistic fashion, without
any base libraries, but just STM32F10x.h, a minimal set of CMSIS headers and a minimal
link script (stm32.ld). This mode can be suitable for writing bootloaders.
Each component is described in more detail below:

## env-stm32.sh

It must be _sourced_ (and not run in a subshell). What it does is to provide a
 thin shell around the cmake command that passes to it the stm32-toolchain.cmake
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
 The env script also provides convenience aliases for the ocmd.sh and flash.sh scripts,
 so they can be invoked as shell functions rather than specifying path and filename.
 That is, instead of running
 
 `/path/to/ocmd.sh <command(s)>`
 you can run:
 `ocmd <command(s)>`
 
## stm32-toolchain.cmake

- Sets up the sysroot for the toolchain to point to the sysroot subdir of this project.

- Based on the option to use or not libopencm3 (`optUseOpenCm3`), and on the type
 of chip used (`optChipFamily`), sets up a global define for the chip type and
 path to a linker script. The toolchain script automatically detects the supported
 by libopencm3 STM32 families(F1, F2, ...) and, based on the selected one, detects
 the supported models and fills up the possible linker script paths.
 
 For the barebones mode (no libopencm3), stm32.ld is used, which may be too
 minimalistic and the user may want to copy and extend it. In barebones mode, the
 script path can be manually set, unline the opencm3 mode where the script path
 is currently a drop down list.

- Semihosting - if not explicitly disabled by optNoSemihosting, and _only_ in debug
 build mode, the linker is directed to use a version of the C libraries that support
 semihosting (`--specs=rdimon.specs -lc`). This enables the use of printf() and
 similar standard functions for output to OpenOCD. `ocmd.sh` always enables semihosting
 support in OpenOCD. Note that semihosting can be implemented without enabling this
 option or using a special version of the standard C library - just with a few lines
 of assembly code that call the BKPT / SVC ARM instruction.

## ocmd.sh
Sends a command to OpenOCD via the telnet port. First the script checks if 
 OpenOCD is running. If not, it is started and the script waits till it starts
 accepting telnet connections. Multiple commands can be sent in one call, but they
 must all be placed in quotes and separated by semicolons.
 To shut down OpenOCD you can send the `shutdown` command. Any call to ocmd.sh will
 then first bring up OpenOCD again and execute the command. For help on usage,
 you can call the scripts without arguments.

## flash.sh
Flashesh the specified file to the chip and either resets or halts it, depending on
a command line option. For help on usage, you can call the script without arguments.
