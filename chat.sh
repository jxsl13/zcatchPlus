#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
tail -1500 $DIR/logfile.log | grep -E "\[chat\]: [0-9]+:"
