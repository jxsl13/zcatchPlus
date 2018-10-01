#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only

back_to_base_directory

# print status
echo "ls" > fifofile
sleep .01
# search for "[I]" in the last 20 lines
RESULT=$(tail -n 20 logfile.log | grep "\[I\]")

#if result contains characters and not whitespace
if [[ $RESULT = *[!\ ]* ]]; then
        echo "show_irregular_flags -1" > fifofile
        sleep .01
fi
tail -48 logfile.log
