#!/bin/bash

# print status
echo "ls" > fifofile
sleep .01
# search for "[I]" in the last 20 lines
RESULT=$(tail -n 20 logfile.log | grep "\[I\]")

#if result contains characters and not whitespace
if [[ $RESULT = *[!\ ]* ]]; then
        echo "show_irregular_flags_all" > fifofile
        sleep .01
fi
tail -34 logfile.log
