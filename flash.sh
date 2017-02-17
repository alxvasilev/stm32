#!/bin/bash

function printUsage
{
    echo -e "\nOpenOCD flash image command. Usage:\n\
    flash.sh [-e|--elf] [-h|--halt] <image[.elf]>\n\
    -e --elf Flash the file as an ELF image. This is implied\n\
    if the file extension is .elf\n\
    -h --halt - Do not reset after flashing the image\n\
    --help Print this help\n"
}

if [ "$#" -lt "1" ]; then
    printUssage
    exit 1
fi

owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`

while [[ $# > 0 ]]
do
    key="$1"
    case $key in
    -e|--elf)
        elf=1
        ;;
    -h|--halt)
        hlt=1
        ;;
    --help)
        printUsage
        exit 0
        ;;
    *)
        if [ ${1:0:1} != '-' ]; then
            if [ ! -z "$fname"]; then
                echo "Only one file name can be specified"
                exit 1
            fi
            fname="$1"
        else
            echo "Unknown option '$1'"
            exit 1
        fi
        ;;
    esac
    shift # past argument or value
done

if [ -z "$fname" ]; then
    echo "No file specified"
    exit 1
fi

if [ ! -f "$fname" ]; then
    echo "File '$fname' not found"
    exit 2
fi

extension="${fname##*.}"
if [ "$extension" == "elf" ] || [ "$elf" == "1" ]; then
    type="elf"
    echo "Flashing ELF file"
else
    type=""
fi

if [ "$hlt" != "1" ]; then
    rst="; reset"
fi

$owndir/ocmd.sh "halt; flash write_image erase $fname 0x08000000 $type$rst"
