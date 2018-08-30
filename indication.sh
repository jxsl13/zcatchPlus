#!/bin/bash
# shows zooming players as per indication list
cat logfile.log | grep "Long_Term_Data\]: Zoom Indication Counter: [1-9+]" -B 2 -A 1