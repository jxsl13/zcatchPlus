#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
. $DIR/function_variable_definitions.sh --source-only



COREDUMP_PATH="core"
BINARY_NAME="zcatch_srv_d"

back_to_base_directory


if [[ "$OSTYPE" == "linux-gnu" ]]; then
       # default is linux

elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX

   	BINARY_NAME="zcatch_srv_x86_d"

    # check if core dumps are saved in the current directory
	if [[ $(sysctl -n kern.corefile) = "/cores/core.%P" ]]; then
		echo "Could not pack the core dump. Cores are not saved in the binary's directory."
		COREDUMP_PATH=""

	elif [[ $(sysctl -n kern.corefile) = "core.%P" ]]; then
		# core dumps are saved in the current directory

		# check if last file changed is the core file:
		FILE_MATCHES=$(ls -Art | tail -n 1 | grep "core.")
		if [[ -f $FILE_MATCHES ]]; then
			COREDUMP_PATH=$FILE_MATCHES
		else
			COREDUMP_PATH=""
		fi

	fi
#elif [[ "$OSTYPE" == "cygwin" ]]; then
	# POSIX compatibility layer and Linux environment emulation for Windows
#elif [[ "$OSTYPE" == "msys" ]]; then
	# Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
#elif [[ "$OSTYPE" == "win32" ]]; then
	# I'm not sure this can happen.
#elif [[ "$OSTYPE" == "freebsd"* ]]; then
	# ...
else
    echo "Not supported operating system."
fi


# check if file exists

if [[ -f $COREDUMP_PATH ]]; then
	# TODO: support macOS as well
	#for now debian only
	NAME=$(date +"%Y_%m_%d_%H-%M-%S")-COREDUMP.tar

	# create dumps folder
	mkdir -p dumps

	# zip binary and core dump
	tar -zcvf $NAME $BINARY_NAME $COREDUMP_PATH

	# move to dumps
	mv $NAME dumps/.

	rm $COREDUMP_PATH

fi


