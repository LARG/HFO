#!/bin/bash

./bin/HFO --fullstate --headless --no-sync --defense-agents=1 --defense-npcs=1 --offense-npcs=1 --trials 30  &
sleep 15
./build/example/high_level_random_agent 6000 &
sleep 5


# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
