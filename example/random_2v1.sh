#!/bin/bash

./bin/HFO --offense-agents=2 --defense-npcs=1 --trials 20 --headless &
sleep 5
./example/high_level_random_agent 6000 &> agent1.txt &
sleep 5
./example/high_level_random_agent 6000 &> agent2.txt &

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
