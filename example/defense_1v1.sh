#!/bin/bash

./bin/HFO --offense-npcs=1 --defense-agents=1 --trials 20 --agent-play-goalie --headless &
sleep 5
./example/example_defense_agent.py &> agent1.txt &
sleep 5

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait