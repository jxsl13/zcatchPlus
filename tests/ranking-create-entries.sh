#!/bin/bash

DATABASE=$1
NUM_ENTRIES=$2

if [ "$DATABASE" = "" -o "$NUM_ENTRIES" = "" ]; then
	echo "Usage: $0 <database-file> <num-random-entries>"
	exit
fi

for (( c = 0; c < $NUM_ENTRIES; c++ )); do
	
	N=$(( $RANDOM % 7 + $RANDOM % 7 + 1 ))
	USERNAME="$( tr -dc "[:alpha:]" < /dev/urandom | head -c $N )"
	SCORE=$(( $RANDOM % 10000 ))
	NUM_WINS=$(( $RANDOM % 100 ))
	NUM_KILLS=$(( $RANDOM % 1000 ))
	NUM_DEATHS=$(( $RANDOM % 1000 ))
	NUM_SHOTS=$RANDOM
	HIGHEST_SPREE=$(( $RANDOM % 16 ))
	TIME_PLAYED=$RANDOM
	
	sqlite3 $DATABASE "
		INSERT OR REPLACE INTO zCatch (
			username, score, numWins, numKills, numDeaths, numShots, highestSpree, timePlayed
		)
		SELECT
			username,
			score + $SCORE,
			numWins + $NUM_WINS,
			numKills + $NUM_KILLS,
			numDeaths + $NUM_DEATHS,
			numShots + $NUM_SHOTS,
			MAX(highestSpree, $HIGHEST_SPREE),
			timePlayed + $TIME_PLAYED
		FROM (SELECT 1)
		LEFT JOIN (
			SELECT *
			FROM zCatch
			WHERE username = \"$USERNAME\"
		);"
	
	sleep 0.005
	
done


