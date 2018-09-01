#!/bin/bash

# downloads bam if necessary, and builds server from source,
# does not clean previous build data.
. ./function_variable_definitions.sh --source-only


check_build_bam
retrieve_cores

if [[ $1 == "-d" ]]; then
    build_debug_server
else
    build_server
fi












