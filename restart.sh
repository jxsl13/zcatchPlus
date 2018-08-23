#!/bin/bash

exec 3<> fifofile

if [[ -p fifofile ]]; then 
echo "say Server Restart due to Update" > fifofile
sleep 1

echo "say 5" > fifofile
sleep 1

echo "say 4" > fifofile
sleep 1

echo "say 3" > fifofile
sleep 1

echo "say 2" > fifofile
sleep 1

echo "say 1" > fifofile
sleep 1

echo "shutdown" > fifofile
fi

if [ -f /usr/bin/byobu ]; then 
byobu kill-session -t "zcatch"
byobu new-session -d -s "zcatch" "./start.sh"
else
echo "Byobu does not exist - please install byobu"
fi
