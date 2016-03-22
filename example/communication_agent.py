#!/usr/bin/env python
# encoding: utf-8

# Before running this program, first Start HFO server:
# $> ./bin/HFO --offense-agents 1

import sys
from hfo import *

if __name__ == '__main__':
  # Create the HFO Environment
  hfo = hfo.HFOEnvironment()
  # Connect to the server with the specified
  # feature set. See feature sets in hfo.py/hfo.hpp.
  hfo.connectToServer(HIGH_LEVEL_FEATURE_SET,
                      'bin/teams/base/config/formations-dt', 6000,
                      'localhost', 'base_left', False)
  for episode in xrange(10):
    status = IN_GAME
    while status == IN_GAME:
      # Grab the state features from the environment
      features = hfo.getState()
      # Get any incoming communication
      msg = hfo.hear()
      # Do something with incoming communication
      print 'Heard: ', msg
      # Take an action and get the current game status
      hfo.act(DASH, 20.0, 0)
      # Do something with outgoing communication
      hfo.say('Message')
      status = hfo.step()
    print 'Episode', episode, 'ended with', hfo.statusToString(status)
