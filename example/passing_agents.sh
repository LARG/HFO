#!/bin/bash

./bin/HFO --offense-agents=2 --no-sync &
sleep 1
./example/communication_agent 7 6000 &
sleep 1
./example/communication_agent 11 6000 &

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
