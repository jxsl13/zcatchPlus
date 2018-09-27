#!/bin/bash

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
CORES="1"

back_to_base_directory (){
	cd $BASE_DIR
}

update_source(){
	echo "Updating source from remote repository"
	git pull
}

clean_previous_build (){
	back_to_base_directory
	../bam/bam -c all
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

build_debug_server (){
    echo "Using $CORES cores to build"
	back_to_base_directory
	../bam/bam -j $CORES -a server_debug
}

build_server (){
    echo "Using $CORES cores to build"
	back_to_base_directory
	../bam/bam -j $CORES -a server_release
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

start_server_debugging (){
        back_to_base_directory

        if [[ "$OSTYPE" == "linux-gnu" ]]; then
        # ...
        ulimit -c unlimited
        ./zcatch_srv_d

        elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        if [[ $(sysctl -n kern.corefile) = "/cores/core.%P" ]]; then
        	echo "To change the dump location to the current folder, please enter your sudo password."
        	echo "If you do not want to change the path, please take note that the core dump tiles will be saved in /cores"
        	echo "Press enter a few times to skip this step"
        	sudo sysctl -w kern.corefile=core.%P
        	echo "Your core dump files can be found at: $(sysctl -n kern.corefile)"

            ulimit -c unlimited
            ./zcatch_srv_x86_d
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

restart_server (){
    back_to_base_directory

    exec 3<> fifofile
    if [[ -p fifofile ]]; then
    echo "say Server Restart due to Update" > fifofile
    sleep 1
    echo "say 5" > fifofile
    sleep 1
    echo "say 4" > fifofile
    sleep 1
    echo "say 3" > fifofile
    sleep 1
    echo "say 2" > fifofile
    sleep 1
    echo "say 1" > fifofile
    sleep 1
    echo "shutdown" > fifofile
    sleep 0.2
    fi

    if [ $(which byobu) ]; then
        byobu kill-session -t "zcatch"
        sleep 0.2
        byobu new-session -d -s "zcatch" "./start.sh $1"
    else
        echo "Byobu does not exist - please install byobu"

        if [[ "$OSTYPE" == "linux-gnu" ]]; then
            # ...
            sudo apt install byobu

        elif [[ "$OSTYPE" == "darwin"* ]]; then
            # Mac OSX
            brew install byobu

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
fi
}


