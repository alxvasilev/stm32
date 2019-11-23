#!/bin/bash

# @author Alexander Vassilev
# @copyright BSD License

export STM32_RED="\033[1;31m"
export STM32_GREEN="\033[0;32m"
export STM32_NOMARK="\033[0;0m"
export STM32_BOLD="\033[30;1m"

owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`
export STM32_ENV_DIR=$owndir

if [ "$#" != "1" ]; then
    STM32_SYSROOT="$owndir/sysroot"
else
    STM32_SYSROOT=`readlink -f $1`
fi

if [ ! -d "$STM32_SYSROOT" ]; then
   echo -e "${STM32_RED}sysroot '$STM32_SYSROOT' dir does not exist${STM32_NOMARK}"
   return 2
fi

function xcmake
{
    cmake "-DCMAKE_TOOLCHAIN_FILE=$owndir/stm32-toolchain.cmake" "$@"
}
export -f xcmake

function ecmake
{
    cmake "-DCMAKE_MODULE_PATH=$owndir" "$@"
}
export -f ecmake

function ocmd
{
    $owndir/ocmd.sh "$@"
}
export -f ocmd

function flash
{
    $owndir/flash.sh "$@"
}
export -f flash

function hc05
{
    if [ -z "$1" ]; then
        ttyDev=/dev/ttyUSB0
    else
        ttyDev="$1"
    fi
    socat "$ttyDev",b38400,raw,echo=0,crnl -
}
export -f hc05

if [ "$USER" == "root" ]; then
    prompt="#"
else
    prompt="\$"
fi

export PS1="[\u@\[\033[0;32m\]\[\033[3m\]stm32\[\033[0m\] \w]$prompt"

# Convenience alias
alias gdb=arm-none-eabi-gdb
alias objcopy=arm-none-eabi-objcopy
alias gcc=arm-none-eabi-gcc
alias g++=arm-none-eabi-g++
alias as=arm-none-eabi-as
alias nm=arm-none-eabi-nm
alias strip=arm-none-eabi-strip

echo -e "\
===================================================================
Your environment has been set up for STM32 cross-compilation and emulation.
${STM32_GREEN}STM32_ENV_DIR${STM32_NOMARK}=$owndir
Use '${STM32_GREEN}xcmake${STM32_NOMARK}' instead of 'cmake' in order to configure project for
cross-compilation
Use '${STM32_GREEN}flash${STM32_NOMARK}' to flash chip, see flash --help for details
Use '${STM32_GREEN}ocmd${STM32_NOMARK} <commands>' to send any command to OpenOCD.
Use '${STM32_GREEN}ecmake${STM32_NOMARK}' instead of 'cmake' in order to configure project for emulation.
The project should '${STM32_GREEN}include(stm32++-emulation)${STM32_NOMARK}'. It will use the native
PC toolchain and build a PC executable. This can be used for development and
debugging of hardware-independent parts of firmware code as a PC application
${STM32_BOLD}Enjoy programming!${STM32_NOMARK}
==================================================================="
