#!/bin/bash

exec 3<> fifofile

if [[ -p fifofile ]]; then 
echo "Server Restart due to Update" > fifofile
sleep 1

echo "5" > fifofile
sleep 1

echo "4" > fifofile
sleep 1

echo "3" > fifofile
sleep 1

echo "2" > fifofile
sleep 1

echo "1" > fifofile
sleep 1
fi

if [ -f /usr/bin/byobu ]; then 
byobu kill-session -t "zcatch"
byobu new-session -d -s "zcatch" "./start.sh"
else
echo "Byobu does not exist - please install byobu"
fi
