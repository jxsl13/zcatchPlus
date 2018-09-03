#!/bin/bash
. ./function_variable_definitions.sh --source-only

if [[ $1 = "-d" ]]; then
	restart_server $1
elif [[ $1 = "-n" ]]; then
	restart_server
else
	echo "Please use either -d for debugging restart or -n for a normal restart!"
fi



