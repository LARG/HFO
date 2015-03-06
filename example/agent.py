#!/usr/bin/env python
# encoding: utf-8

import imp

if __name__ == '__main__':
  # First Start the server by calling start.py in bin
  # Load the HFO library
  hfo_module = imp.load_source('HFO', '../HFO.py')
  # Create the HFO Environment
  hfo = hfo_module.HFOEnvironment()
  hfo.connectToAgentServer()
  # Continue until finished
  while True:
    # Grab the state features from the environment
    features = hfo.getState()
    # Take an action and get the reward
    reward = hfo.act(0)
  # Cleanup when finished
  hfo.cleanup()
