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
	NUM_KILLS_WALLSHOT=$(( $RANDOM % 100 ))
	NUM_DEATHS=$(( $RANDOM % 1000 ))
	NUM_SHOTS=$RANDOM
	HIGHEST_SPREE=$(( $RANDOM % 16 ))
	TIME_PLAYED=$RANDOM
	
	sqlite3 $DATABASE "
		INSERT OR REPLACE INTO zCatch (
			username, score, numWins, numKills, numKillsWallshot, numDeaths, numShots, highestSpree, timePlayed
		)
		SELECT
			new.username,
			COALESCE(old.score, 0) + $SCORE,
			COALESCE(old.numWins, 0) + $NUM_WINS,
			COALESCE(old.numKills, 0) + $NUM_KILLS,
			COALESCE(old.numKillsWallshot, 0) + $NUM_KILLS_WALLSHOT,
			COALESCE(old.numDeaths, 0) + $NUM_DEATHS,
			COALESCE(old.numShots, 0) + $NUM_SHOTS,
			MAX(COALESCE(old.highestSpree, 0), $HIGHEST_SPREE),
			COALESCE(old.timePlayed, 0) + $TIME_PLAYED
		FROM (
			SELECT \"$USERNAME\" as username
		) new
		LEFT JOIN (
			SELECT *
			FROM zCatch
		) old ON old.username = new.username;"
	
	sleep 0.005
	
done


