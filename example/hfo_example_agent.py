#!/usr/bin/env python
# encoding: utf-8

import sys

# First Start the server: $> bin/start.py

if __name__ == '__main__':


  port = 6000
  if len(sys.argv) > 1:
    port = int(sys.argv[1])
  try:
    from hfo import *
  except:
    print 'Failed to import hfo. To install hfo, in the HFO directory'\
      ' run: \"pip install .\"'
    exit()
  # Create the HFO Environment
  hfo = hfo.HFOEnvironment()
  # Connect to the agent server on port 6000 with the specified
  # feature set. See feature sets in hfo.py/hfo.hpp.
  hfo.connectToAgentServer(port, HFO_Features.HIGH_LEVEL_FEATURE_SET)
  # Play 5 episodes
  for episode in xrange(5):
    status = HFO_Status.IN_GAME
    while status == HFO_Status.IN_GAME:
      # Grab the state features from the environment
      features = hfo.getState()
      # Take an action and get the current game status
      hfo.act(HFO_Actions.DASH, 20.0, 0)
      hfo.step()
      
    
    print 'Episode', episode, 'ended with',
    # Check what the outcome of the episode was
    if status == HFO_Status.GOAL:
      print 'goal'
    elif status == HFO_Status.CAPTURED_BY_DEFENSE:
      print 'captured by defense'
    elif status == HFO_Status.OUT_OF_BOUNDS:
      print 'out of bounds'
    elif status == HFO_Status.OUT_OF_TIME:
      print 'out of time'
    else:
      print 'Unknown status', status
      exit()
  # Cleanup when finished
  hfo.cleanup()
