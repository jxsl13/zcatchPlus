#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only


# todo: add macOS
COREDUMP_PATH="core"

back_to_base_directory

if [[ -f $COREDUMP_PATH ]]; then
	# TODO: support macOS as well
	#for now debian only
	NAME=$(date +"%Y_%m_%d_%H-%M-%S")-COREDUMP.tar

	# create dumps folder
	mkdir -p dumps

	# zip binary and core dump
	tar -zcvf $NAME zcatch_srv_d $COREDUMP_PATH

	# move to dumps
	mv $NAME dumps/.

	rm $COREDUMP_PATH

fi


