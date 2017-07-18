#!/bin/bash

./bin/HFO --offense-npcs=2 --defense-npcs=2 --trials 5000 --headless  --seed=1500348586 --no-logging &

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
