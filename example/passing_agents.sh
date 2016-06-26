#!/bin/bash

./bin/HFO --offense-agents=2 --no-sync --fullstate &
sleep 5
./example/communication_agent 6000 &
sleep 5
./example/communication_agent 6000 &

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait
