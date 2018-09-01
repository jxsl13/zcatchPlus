#!/bin/bash

# downloads bam if necessary, and builds server from source
. ./function_variable_definitions.sh --source-only

check_build_bam
clean_previous_build
retrieve_cores
build_server











