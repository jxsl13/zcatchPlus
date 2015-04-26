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
		INSERT OR REPLACE INTO zCatchScore ( \
			username, \
			score, \
			numWins, \
			numKills, \
			numDeaths, \
			numShots, \
			highestSpree, \
			timePlayed \
		) VALUES ( \
			\"$USERNAME\", \
			COALESCE((SELECT score FROM zCatchScore WHERE username = \"$USERNAME\"), 0) + $SCORE, \
			COALESCE((SELECT numWins FROM zCatchScore WHERE username = \"$USERNAME\"), 0) + $NUM_WINS, \
			COALESCE((SELECT numKills FROM zCatchScore WHERE username = \"$USERNAME\"), 0) + $NUM_KILLS, \
			COALESCE((SELECT numDeaths FROM zCatchScore WHERE username = \"$USERNAME\"), 0) + $NUM_DEATHS, \
			COALESCE((SELECT numShots FROM zCatchScore WHERE username = \"$USERNAME\"), 0) + $NUM_SHOTS, \
			MAX(COALESCE((SELECT highestSpree FROM zCatchScore WHERE username = \"$USERNAME\"), 0), $HIGHEST_SPREE), \
			COALESCE((SELECT timePlayed FROM zCatchScore WHERE username = \"$USERNAME\"), 0) + $TIME_PLAYED \
		);"
	
	sleep 0.005
	
done


