#!/usr/bin/env python
# encoding: utf-8

# Before running this program, first Start HFO server:
# $> ./bin/HFO --offense-agents 1

import random
from hfo import *

def main():
  # Create the HFO Environment
  hfo = HFOEnvironment()
  # Connect to the server with the specified
  # feature set. See feature sets in hfo.py/hfo.hpp.
  hfo.connectToServer(HIGH_LEVEL_FEATURE_SET,
                      'bin/teams/base/config/formations-dt', 11, 6000,
                      'localhost', 'base_left', False)
  for episode in xrange(10):
    status = IN_GAME
    while status == IN_GAME:
      state = hfo.getState()
      if state[5] == 1: # State[5] is 1 when the player can kick the ball
        hfo.act(random.choice([SHOOT, DRIBBLE]))
      else:
        hfo.act(MOVE)
      status = hfo.step()
    # print 'Episode', episode, 'ended with', hfo.statusToString(status)
    print('Episode %d ended with %s'%(episode, hfo.statusToString(status)))

if __name__ == '__main__':
  main()
