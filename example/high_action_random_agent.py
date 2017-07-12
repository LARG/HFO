#!/usr/bin/env python
from __future__ import print_function
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
  hfo_env.connectToServer(hfo.LOW_LEVEL_FEATURE_SET,
                          'bin/teams/base/config/formations-dt', args.port,
                          'localhost', 'base_left', False)
  if args.seed:
    print("Python randomization seed: {0:d}".format(args.seed))
  for episode in itertools.count():
    num_reorient = 0
    num_move = 0
    num_had_ball = 0
    status = hfo.IN_GAME
    while status == hfo.IN_GAME:
      # Get the vector of state features for the current state
      state = hfo_env.getState()
      # Perform the action
      if state[12] > 0: # State[12] is 1 when the player can kick the ball
        if random.random() < 0.5: # more efficient than random.choice for 2
          hfo_env.act(hfo.SHOOT)
        else:
          hfo_env.act(hfo.DRIBBLE)
        num_had_ball += 1
      # 8 is frozen; rest are self or ball position/velocity valid
      elif (state[8] > 0) or (min(state[0],state[1],state[50],state[54]) < 0):
        hfo_env.act(hfo.REORIENT)
        num_reorient += 1
      else:
        hfo_env.act(hfo.MOVE)
        num_move += 1
      # Advance the environment and get the game status
      status = hfo_env.step()
      
    # Check the outcome of the episode
    print("Episode {0:d} ended with status {1}".format(episode,
                                                       hfo_env.statusToString(status)))
    print("\tHad ball: {0:d}; Reorient: {1:d}; Move: {2:d}".format(num_had_ball,
                                                                   num_reorient,
                                                                   num_move))
    # Quit if the server goes down
    if status == hfo.SERVER_DOWN:
      hfo_env.act(hfo.QUIT)
      exit()

if __name__ == '__main__':
  main()
