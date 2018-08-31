#!/bin/bash
# updates source and restarts the server with the updated build
BASE_DIR=$(pwd)
CORES="1"

build_server (){
	back_to_base_directory
	../bam/bam -j $CORES server_release
}

clean_previous_build (){
	back_to_base_directory
	../bam/bam -c all
}

start_server (){
	back_to_base_directory

	if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        ./zcatch_srv

	elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        ./zcatch_srv_x86

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
    echo "Updated source from remote repository"
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

backup_server_config(){
    back_to_base_directory
    if [ -f autoexec.cfg ]; then
        cp autoexec.cfg ../.
        git checkout autoexec.cfg
        echo "Backed up autoexec.cfg"
    else
        echo "Could not backup autoexec.cfg, file not found"
    fi
}

restore_server_config(){
    back_to_base_directory
    if [ -f ../autoexec.cfg ]; then
        mv ../autoexec.cfg .
        echo "Restored autoexec.cfg"
    else
        echo "Could not restore autoexec.cfg, backup not found"
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

backup_server_config
update_source
restore_server_config

check_build_bam
retrieve_cores
clean_previous_build
build_server
start_server









