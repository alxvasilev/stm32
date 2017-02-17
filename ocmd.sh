#!/bin/bash

if [ "$#" != "1" ]; then
    echo -e "OpenOCD command client. Usage:\n\
    ocmd.sh \"<command[;command[;command[...]]]>\""
    exit 1
fi

pid=`pidof openocd`
if [ `pidof openocd > /dev/null; echo $?` != "0" ]; then
    echo "OpenOCD not running, starting it..."
    sudo openocd \
        -f /usr/share/openocd/scripts/interface/stlink-v2.cfg \
        -f /usr/share/openocd/scripts/target/stm32f1x.cfg \
        -c "log_output /var/log/openocd.log" &
    sleep 1
    while [ `nc -z -w5 localhost 4444; echo $?` != "0" ]
    do
        sleep 1
    done
    echo "OpenOCD telnet port detected, proceding with command"
fi

echo "$1" | nc -T -c localhost 4444
