#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
cat $DIR/logfile.log | grep "Showing long term data of player " -A 13
