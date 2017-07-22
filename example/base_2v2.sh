#!/bin/bash

./bin/HFO --offense-npcs=2 --defense-npcs=2 --trials 20 --headless &

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
