#!/bin/bash

# downloads bam if necessary, and builds server from source
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only
back_to_base_directory


check_build_bam
clean_previous_build
retrieve_cores
build_server











