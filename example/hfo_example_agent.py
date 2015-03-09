#!/usr/bin/env python
# encoding: utf-8

import imp

# First Start the server: $> bin/start.py

if __name__ == '__main__':
  # Load the HFO library
  try:
    hfo_module = imp.load_source('HFO', '../HFO.py')
  except:
    hfo_module = imp.load_source('HFO', 'HFO.py')
  # Get the possible actions
  HFO_Actions = hfo_module.HFO_Actions
  # Get the possible outcomes
  HFO_Status = hfo_module.HFO_Status
  # Create the HFO Environment
  hfo = hfo_module.HFOEnvironment()
  # Connect to the agent server
  hfo.connectToAgentServer()
  # Play 5 episodes
  for episode in xrange(5):
    status = HFO_Status.IN_GAME
    while status == HFO_Status.IN_GAME:
      # Grab the state features from the environment
      features = hfo.getState()
      # Take an action and get the current game status
      status = hfo.act((HFO_Actions.KICK, 100, 12.3))
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
