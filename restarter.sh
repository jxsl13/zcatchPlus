#!/usr/bin/env bash
# Example: ./restarter 8303 ./teeworlds_srv -f config.cfg
if (( $# < 3 )); then
    echo Usage: "$0" SRV_PORT COMMAND [PARAM]...
    exit 1
fi

# record the port
PORT=$1
# and shift the parameters so we can use $@ as the executed command
shift 1

# check whether the server is still responding
# first and only parameter is the PID of the process to kill
response_check() {
    PID=$1
    
    # initial sleep
    sleep 5

    while true; do
        echo :: Checking
        # query the server for info
        # check response length
        NOT0="$(echo -n $'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xffgie3\xff' | netcat -u -q 1 127.0.0.1 $PORT | wc -c)"

        # if response length is zero, kill the process
        if [ x"$NOT0" == x"0" ]; then
            echo :: KILLING
            # try to kill it
            kill $PID

            # wait a bit and finish it off
            sleep 0.5
            kill -s KILL $PID

            return
        fi
        echo :: Everything fine

        # sleep after each check
        sleep 3
    done
}

# kill all subprocesses on exit
trap 'kill $(jobs -p)' EXIT

# loop forever
while true; do
    # execute and record PID
    echo :: Executing "$@"
    "$@" &
    PID=$!
    echo :: PID $PID

    # launch response check
    response_check $PID &
    RC_PID=$!
    echo :: RC_PID $RC_PID

    # wait for the teeworlds process to end/crash/be killed
    wait $PID
    echo :: Process ended

    # kill the response check
    kill -s KILL $RC_PID

    echo :: Sleeping
    # sleep so we don't use too much CPU if the server instantly crashes
    sleep 1
done
