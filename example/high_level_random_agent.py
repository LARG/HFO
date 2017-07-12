#!/usr/bin/env python
# encoding: utf-8

# Before running this program, first Start HFO server:
# $> ./bin/HFO --offense-agents 1

import argparse
import itertools
import random
try:
  import hfo
except ImportError:
  print('Failed to import hfo. To install hfo, in the HFO directory'\
    ' run: \"pip install .\"')
  exit()

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000,
                      help="Server port")
  parser.add_argument('--seed', type=int, default=None,
                      help="Python randomization seed; uses python default if 0 or not given")
  args=parser.parse_args()
  if args.seed:
    random.seed(args.seed)
  # Create the HFO Environment
  hfo_env = hfo.HFOEnvironment()
  # Connect to the server with the specified
  # feature set. See feature sets in hfo.py/hfo.hpp.
  hfo_env.connectToServer(hfo.HIGH_LEVEL_FEATURE_SET,
                          'bin/teams/base/config/formations-dt', args.port,
                          'localhost', 'base_left', False)
  for episode in itertools.count():
    status = hfo.IN_GAME
    while status == hfo.IN_GAME:
      # Get the vector of state features for the current state
      state = hfo_env.getState()
      # Perform the action
      if state[5] == 1: # State[5] is 1 when the player can kick the ball
        if random.random() < 0.5: # more efficient than random.choice for 2
          hfo_env.act(hfo.SHOOT)
        else:
          hfo_env.act(hfo.DRIBBLE)
      else:
        hfo_env.act(hfo.MOVE)
      # Advance the environment and get the game status
      status = hfo_env.step()
      
    # Check the outcome of the episode
    print(('Episode %d ended with %s'%(episode,
                                       hfo_env.statusToString(status))))
    # Quit if the server goes down
    if status == hfo.SERVER_DOWN:
      hfo_env.act(hfo.QUIT)
      exit()

if __name__ == '__main__':
  main()
