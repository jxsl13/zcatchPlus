#!/bin/bash

tail -400 logfile.log | grep -E "\[chat\]: [0-9]+:"
