#!/bin/bash

BASE_DIR=$(pwd)

build_debug_server (){
	back_to_base_directory
	../bam/bam server_debug
}

clean_previous_build (){
	back_to_base_directory
	../bam/bam -c all
}

start_server_debugging (){
	back_to_base_directory

	if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        gdb ./zcatch_srv_d --command 'gdb_commands.txt'

	elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        gdb ./zcatch_srv_x86_d --command 'gdb_commands.txt'

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
}

back_to_base_directory (){
	cd $BASE_DIR
}

update_source(){
	git pull
}

check_build_bam (){
	if [ ! -f ../bam/bam ]; then
    # Download and compile bam:
    cd ..
    git clone https://github.com/matricks/bam.git bam
    cd bam

    if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        chmod +x make_unix.sh
        ./make_unix.sh
        chmod +x bam

	elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        chmod +x make_unix.sh
        ./make_unix.sh
        chmod +x bam

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

	back_to_base_directory

fi
}


update_source
check_build_bam

if [[ $1 == "-c" ]]; then
    clean_previous_build
fi

build_debug_server
start_server_debugging









