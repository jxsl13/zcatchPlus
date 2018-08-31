#!/bin/bash

BASE_DIR=$(pwd)
CORES="1"

build_debug_server (){
	back_to_base_directory
	../bam/bam -j $CORES -a server_debug
}


clean_previous_build (){
	back_to_base_directory
	../bam/bam -c all
}

start_server_debugging (){
        back_to_base_directory

        if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        ulimit -c unlimited
        ./zcatch_srv_d

        elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        echo "To change the dump location to the current folder, please enter your sudo password."
        echo "If you do not want to change the path, please take note that the core dump tiles will be saved in /cores"
        sudo sysctl -w kern.corefile=core.%P
        ulimit -c unlimited
        ./zcatch_srv_x86_d

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


update_source
check_build_bam

if [[ $1 == "-c" ]]; then
    clean_previous_build
fi
retrieve_cores
echo $CORES
build_debug_server
start_server_debugging









