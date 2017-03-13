#!/bin/bash
# @author Alexander Vassilev
# @copyright BSD License

STM_VERSION=1

if [ "$#" -lt "1" ]; then
    echo -e "OpenOCD command client. Usage:\n\
    ocmd.sh \"<command[;command[;command[...]]]>\""
    exit 1
fi

owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`
in="$owndir/openocd.stdin"
# out="$owndir/openocd.stdout"

pid=`pidof openocd`
if [ `pidof openocd > /dev/null; echo $?` != "0" ]; then
    if [ ! -p "$in" ]; then
        echo "Creating named pipe '$in' for openOCD and semihosting stdin"
        rm -f "$in"
        mkfifo "$in"
    fi

    echo "OpenOCD not running, starting it..."
    openocd \
        -f /usr/share/openocd/scripts/interface/stlink-v2.cfg \
        -f /usr/share/openocd/scripts/target/stm32f${STM_VERSION}x.cfg \
        -c 'init; arm semihosting enable' < "$in" &

    # opening fifo pipes blocks until the other end is opened, so openocd
    # will not be started unless we open the pipe

    sleep 0.5
    touch "$in"

    while [ `nc -z -w5 localhost 4444; echo $?` != "0" ]
    do
        echo "Waiting for telnet port..."
        sleep 1
        touch "$in"
    done
    echo "OpenOCD telnet port detected, proceeding with command"
fi

# exit should go on another line, because if there is an error in the user
# command, the exit command will be ignored
echo -e "$@\nexit" | nc -T localhost 4444
exit 0
