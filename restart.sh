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

if [ $(which byobu) ]; then 
byobu kill-session -t "zcatch"
byobu new-session -d -s "zcatch" "./start.sh"
else
echo "Byobu does not exist - please install byobu"

if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        sudo apt install byobu

        elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        brew install byobu

        #elif [[ "$OSTYPE" == "cygwin" ]]; then
        # POSIX compatibility layer and Linux environment emulation for Windows
        #elif [[ "$OSTYPE" == "msys" ]]; then
        # Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
        #elif [[ "$OSTYPE" == "win32" ]]; then
        # I'm not sure this can happen.
        #elif [[ "$OSTYPE" == "freebsd"* ]]; then
        # ...
        else
                echo "Not supported operating system."
        fi
fi

