#!/bin/bash

# downloads bam if necessary, and builds server from source
BASE_DIR=$(pwd)
CORES="1"


back_to_base_directory (){
	cd $BASE_DIR
}

build_server (){
	back_to_base_directory
	../bam/bam -j $CORES -a server_release
}

clean_previous_build (){
	back_to_base_directory
	../bam/bam -c all
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

retrieve_cores(){
    if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        CORES=$(grep -c ^processor /proc/cpuinfo)
        if [[ $CORES == "" ]]; then
            CORES="1"
        fi

    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        CORES=$(sysctl -n hw.ncpu)
        if [[ CORES == "" ]]; then
            CORES="1"
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
}


check_build_bam
clean_previous_build
retrieve_cores
build_server











