#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only


if [[ $1 = "-d" ]]; then
	start_server_debugging
else
	start_server
fi

