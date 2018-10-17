#!/bin/bash

# parameters (part)
# --port=6000

# --offense-npcs= 
# --defense-npcs=
# --offense-agents=
# --defense-agents=

# --trials=-1
# --untouched-time=-1
# --frames-per-trial=1000

# --offense-on-ball=
# --ball-x-min= [0,1]
# --ball-x-max=
# --ball-y-min= [-1,1]
# --ball-y-max=

# --fullstate
# --headless
# --no-sync 
# --no-logging

# 1npc v 1npc
./bin/HFO --port=6000 --offense-npcs=1 --defense-npcs=1 --no-sync --no-logging --trials=5

# 1p
./bin/HFO --port=6000 --offense-agents=1 --untouched-time=-1 --trials=-1 --frames-per-trial=1000 --fullstate --no-logging --no-sync
