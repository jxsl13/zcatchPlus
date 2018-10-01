#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only

back_to_base_directory

cat logfile.log | grep "Showing long term data of player " -A 13
