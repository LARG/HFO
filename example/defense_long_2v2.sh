#!/bin/bash

# HAS TO BE RUN FROM EXAMPLE DIR DUE TO hand_coded_defense_agent CONFIG!

# Change to a new seed for different experiments!

../bin/HFO --offense-npcs=2 --defense-agents=1 --defense-npcs=1 --trials 5000 --headless --port=7000 --seed 1500348586 --no-logging &
sleep 15
./hand_coded_defense_agent &> agent1.txt &
sleep 5

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait