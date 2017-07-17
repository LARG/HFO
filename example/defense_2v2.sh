#!/bin/bash

# HAS TO BE RUN FROM EXAMPLE DIR DUE TO hand_coded_defense_agent CONFIG!

../bin/HFO --offense-npcs=2 --defense-agents=1 --defense-npcs=1 --trials 20 --headless --port=7000 &
# The below sleep period is needed to avoid the agent connecting in before the
# Trainer.py script gets the base/Helios goalie connected in; if that happens,
# the agent gets assigned unum 1 and there is a mixup in which agent is
# supposed to be the goalie (some portions of the various programs go by unum,
# others go by a goalie flag).
sleep 15
./hand_coded_defense_agent &> agent1.txt &
sleep 5

# The magic line
#   $$ holds the PID for this script
#   Negation means kill by process group id instead of PID
trap "kill -TERM -$$" SIGINT
wait