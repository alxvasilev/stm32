#!/bin/bash

# @author Alexander Vassilev
# @copyright BSD License

owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`

if [ "$#" != "1" ]; then
    STM32_SYSROOT="$owndir/sysroot"
else
    STM32_SYSROOT=`readlink -f $1`
fi

if [ ! -d "$STM32_SYSROOT" ]; then
   echo -e "\033[1;31msysroot '$STM32_SYSROOT' dir does not exist\033[0m"
   return 2
fi

STM32_CMAKE_TOOLCHAIN="$owndir/stm32-toolchain.cmake"
export CMAKE_XCOMPILE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$STM32_CMAKE_TOOLCHAIN"

function xcmake
{
    cmake $CMAKE_XCOMPILE_ARGS "$@"
}
export -f xcmake

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

if [ "$USER" == "root" ]; then
    prompt="#"
else
    prompt="\$"
fi

export PS1="[\u@\[\033[0;32m\]\[\033[3m\]stm32\[\033[0m\] \W]$prompt"

# Convenience alias
alias gdb=arm-none-eabi-gdb
alias objcopy=arm-none-eabi-objcopy
alias gcc=arm-none-eabi-gcc
alias g++=arm-none-eabi-g++
alias as=arm-none-eabi-as
alias nm=arm-none-eabi-nm
alias strip=arm-none-eabi-strip

echo -e "Your environment has been set up for STM32 cross-compilation.\n\n\
SYSROOT=$STM32_SYSROOT\n
Use the 'xcmake' command instead of 'cmake' in order to configure project for\n\
cross-compilation\n\
Use the 'flash' command to flash chip, see flash --help for details\n\
Use 'ocmd <commands>' to send any command to OpenOCD.\n"
