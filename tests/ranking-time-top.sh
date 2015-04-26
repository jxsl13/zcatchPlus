#!/bin/bash

DATABASE=$1

if [ "$DATABASE" = "" ]; then
	echo "Usage: $0 <database-file>"
	exit
fi

echo -n "Results returned: "
time sqlite3 $DATABASE "SELECT username, score FROM zCatch ORDER BY score DESC LIMIT 5;" | wc -l
