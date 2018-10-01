#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only

back_to_base_directory

tail -1500 logfile.log | grep -E "\[chat\]: [0-9]+:"
