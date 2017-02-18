owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`

if [ "$#" != "1" ]; then
    STM32_SYSROOT=$owndir
else
    STM32_SYSROOT=`readlink -f $1`
fi

if [ ! -d "$STM32_SYSROOT" ]; then
   echo -e "\033[1;31msysroot '$STM32_SYSROOT' dir does not exist\033[0m"
   return 2
fi

STM32_CMAKE_TOOLCHAIN="$STM32_SYSROOT/stm32-toolchain.cmake"
export CMAKE_XCOMPILE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$STM32_CMAKE_TOOLCHAIN"

function xcmake
{
    cmake $CMAKE_XCOMPILE_ARGS "$@"
}

function ocmd
{
    $owndir/ocmd.sh "$@"
}

function flash
{
    $owndir/flash.sh "$@"
}

# Convenience alias
alias gdb="arm-none-eabi-gdb"

echo -e "Your environment has been set up for STM32 cross-compilation.\n\n\
SYSROOT=$STM32_SYSROOT\n
Use the 'xcmake' command instead of 'cmake' in order to configure project for\n\
cross-compilation\n\
Use the 'flash' command to flash chip, see flash --help for details\n\
Use 'ocmd <command[;command[...]]>' to send any command to OpenOCD.\n"
