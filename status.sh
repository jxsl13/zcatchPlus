#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
# print status
echo "ls" > $DIR/fifofile
sleep .01
# search for "[I]" in the last 20 lines
RESULT=$(tail -n 20 $DIR/logfile.log | grep "\[I\]")

#if result contains characters and not whitespace
if [[ $RESULT = *[!\ ]* ]]; then
        echo "show_irregular_flags -1" > $DIR/fifofile
        sleep .01
fi
tail -48 $DIR/logfile.log
