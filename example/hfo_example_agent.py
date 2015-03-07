#!/usr/bin/env python
# encoding: utf-8

import imp

# First Start the server by calling start.py in bin

if __name__ == '__main__':
  # Load the HFO library
  hfo_module = imp.load_source('HFO', '../HFO.py')
  # Get the possible actions
  actions = hfo_module.Actions
  # Create the HFO Environment
  hfo = hfo_module.HFOEnvironment()
  hfo.connectToAgentServer()
  # Continue until finished
  while True:
    # Grab the state features from the environment
    features = hfo.getState()
    # Take an action and get the reward
    reward = hfo.act((actions.KICK, 100, 12.3))
  # Cleanup when finished
  hfo.cleanup()
