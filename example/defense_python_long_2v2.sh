#!/bin/bash

# Change to a different seed for different experiments!

./bin/HFO --offense-npcs=2 --defense-agents=1 --defense-npcs=1 --trials 5000 --headless --seed 1500348586 --no-logging --hfo-logging &

# Sleep is needed to make sure doesn't get connected too soon, as unum 1 (goalie)
sleep 15
./example/hand_coded_defense_agent.py &> agent1.txt &
sleep 5

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait