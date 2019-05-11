#!/bin/bash
# @author Alexander Vassilev
# @copyright BSD License

function printUsage
{
    echo -e "\nOpenOCD flash image command. Usage:\n\
    flash.sh [-v|--verify] [-h|--halt] [-o|--offset] <image[.elf]>\n\
    -v --verify - Verify written image\n\
    -h --halt - Do not reset after flashing the image\n\
    -o --offset <offset> - Address at which to start writing\n\
      the image, i.e. 0x08000000\n\
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
    -v|--verify)
        verify="1"
        ;;
    -h|--halt)
        hlt="1"
        ;;
    -o|--offset)
        shift
        offset="$1"
        ;;
    --help)
        printUsage
        exit 0
        ;;
    *)
        if [ ${1:0:1} != '-' ]; then
            if [ ! -z "$fname" ]; then
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

if [ "$hlt" != "1" ]; then
    rst="; reset"
fi
# The 'program' openOCD command prevents semihosting from working after flashing
# (semihosting requests hang), until openOCD is restarted. That's why we don't use that
# command
fname=`readlink -f "$fname"`

cmd="reset halt; flash write_image erase $fname"

if [ ! -z "offset" ]; then
    cmd="$cmd $offset"
fi

if [ "$verify" == "1" ]; then
    cmd="$cmd; verify_image $fname"
fi

if [ "$hlt" != "1" ]; then
    cmd="$cmd; reset run"
fi

$owndir/ocmd.sh "$cmd"
