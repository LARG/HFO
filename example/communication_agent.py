#!/usr/bin/env python
# encoding: utf-8

# Before running this program, first Start HFO server:
# $> ./bin/HFO --offense-agents 1

import sys, itertools
from hfo import *

def main():
  # Create the HFO Environment
  hfo = HFOEnvironment()
  # Connect to the server with the specified
  # feature set. See feature sets in hfo.py/hfo.hpp.
  hfo.connectToServer(HIGH_LEVEL_FEATURE_SET,
                      'bin/teams/base/config/formations-dt', 6000,
                      'localhost', 'base_left', False)
  for episode in itertools.count():
    status = IN_GAME
    while status == IN_GAME:
      # Grab the state features from the environment
      features = hfo.getState()
      # Get any incoming communication
      msg = hfo.hear()
      # Print the incoming communication
      if msg:
        print(('Heard: %s'% msg))
      # Take an action
      hfo.act(DASH, 20.0, 0.)
      # Create outgoing communication
      # the message can contain charachters a-z A-Z 0-9 
      # the message can contain special charachters like ?SPACE-*()+_<>/
      # the message cannot contain !@#$^&={}[];:"'
      hfo.say('Hello World')

      # Advance the environment and get the game status
      status = hfo.step()
    # Check the outcome of the episode
    print(('Episode %d ended with %s'%(episode, hfo.statusToString(status))))
    # Quit if the server goes down
    if status == SERVER_DOWN:
      hfo.act(QUIT)
      exit()

if __name__ == '__main__':
  main()
