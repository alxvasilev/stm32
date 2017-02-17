#!/bin/bash

if [ "$#" != "1" ]; then
    echo -e "OpenOCD flash image command. Usage:\n\
    flash.sh <image[.elf]>"
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "File '$1' not found"
    exit 2
fi

owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`

extension="${1##*.}"
if [ "$extension" == "elf" ]; then
    type="elf"
    echo "Flashing ELF file"
else
    type=""
fi

$owndir/ocmd.sh "halt; flash write_image erase $1 0x08000000 $type; reset"
