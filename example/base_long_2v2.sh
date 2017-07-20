#!/bin/bash

# Be sure to change/remove the seed for different experiments!

./bin/HFO --offense-npcs=2 --defense-npcs=2 --trials 5000 --headless  --seed=1500348586 --no-logging --hfo-logging &

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
