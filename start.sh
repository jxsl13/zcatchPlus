#!/bin/bash
. ./function_variable_definitions.sh --source-only


if [[ $1 = "-d" ]]; then
	start_server_debugging
else
	start_server
fi

