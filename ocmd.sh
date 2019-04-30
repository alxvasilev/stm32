#!/bin/bash
# @author Alexander Vassilev
# @copyright BSD License

MARK="\033[1;30m"
NOMARK="\033[0;0m"
ERR="\033[0;31m"

STM_VERSION=1_bluepill

if [ "$#" -lt "1" ]; then
    echo -e "OpenOCD command client. Usage:\n\
    ocmd.sh \"<command[;command[;command[...]]]>\""
    exit 1
fi

owndir=`echo "$(cd $(dirname "${BASH_SOURCE[0]}"); pwd)"`
in="$owndir/openocd.stdin"
#out="$owndir/openocd.stdout"

pid=`pidof openocd`
if [ -z $pid ]; then
    if [ ! -p "$in" ]; then
        echo -e "${MARK}Creating named pipe '$in' for openOCD and semihosting stdin${NOMARK}"
        rm -f "$in"
        mkfifo "$in"
    fi

    echo -e "${MARK}OpenOCD not running, starting it and waiting for it to open telnet port...${NOMARK}"
    openocd \
        -f /usr/share/openocd/scripts/interface/stlink-v2.cfg \
        -f /usr/share/openocd/scripts/target/stm32f${STM_VERSION}x.cfg \
        -c 'init; arm semihosting enable' < "$in" &

    # opening fifo pipes blocks until the other end is opened, so openocd
    # will not be started unless we open the pipe
    # Wait a bit till process initializes and the pipe is opened on its side
    sleep 0.1
    touch "$in"

    # wait a bit more for openocd to start telnet server
    sleep 0.5

    checks=0
    while [ `nc -z localhost 4444; echo $?` != "0" ]
    do
        ((checks++))
        if [ "$checks" -gt "60" ]; then
            echo -e "\n${ERR}Timed out waiting for OpenOCD to start${NOMARK}"
            exit 1
        else
            echo -n '.'
        fi
        sleep 0.5
    done
    if [ "$checks" -gt "0" ]; then
        echo -e "\n"
    fi

    pid=`pidof openocd`
    echo -e "${MARK}OpenOCD telnet port detected, openOCD pid is $pid, proceeding with command(s)${NOMARK}"

    # Disable openocd outputting stuff to the terminal that started it
    echo -e "log_output /dev/null\nexit" | (nc localhost 4444 2>&1) > /dev/null
fi

# exit should go on another line, because if there is an error in the user
# command, the exit command will be ignored
echo -e "$@\nexit" | (nc localhost 4444) | tail -n +2

# openocd outputs its response to the console, and this script may terminate
# before openocd has finished printing. In that case, the propmt will appear
# and the openocd output after it, so the user will get confused. To fix that,
# wait a bit before returning to the prompt
sleep 0.1
exit 0
