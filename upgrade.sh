#!/bin/bash
# updates source and restarts the server with the updated build
. ./function_variable_definitions.sh --source-only


backup_server_config
update_source
restore_server_config

check_build_bam
retrieve_cores
clean_previous_build


if [[ $1 = "-d" ]]; then
    build_debug_server
    restart_server -d
else
    build_server
    restart_server
fi










