#!/bin/bash

# print status
echo "ls" > fifofile;
# last 40 lines
LINES=$(tail -40 logfile.log)
#search for stuff in there
RESULT= "$LINES" | grep -q "\[I\]"

# if stuff is found, do other stuff
if $RESULT; then
	echo "show_irregular_flags_all" > fifofile;
fi