#!/bin/bash

tail -1500 logfile.log | grep -E "\[chat\]: [0-9]+:"
