#!/bin/bash

./bin/HFO --offense-npcs=2 --defense-agents=1 --defense-npcs=1 --trials 5000 --headless --seed 1500310928 --no-logging &
sleep 15
./example/hand_coded_defense_agent.py &> agent1.txt &
sleep 5

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait