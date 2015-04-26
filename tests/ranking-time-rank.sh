#!/bin/bash

DATABASE=$1
USERNAME=$2

if [ "$DATABASE" = "" -o "$USERNAME" = "" ]; then
	echo "Usage: $0 <database-file> <query-username>"
	exit
fi

echo -n "Results returned: "
time sqlite3 $DATABASE "SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed, \
			(SELECT COUNT(*) FROM zCatch b WHERE b.score > a.score) + 1, \
			MAX(0, (SELECT MIN(b.score) FROM zCatch b WHERE b.score > a.score) - a.score) \
		FROM zCatch a \
		WHERE username = \"$USERNAME\" \
	" | wc -l
