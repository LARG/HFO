#!/usr/bin/env python
# encoding: utf-8

# First Start the server: $> bin/start.py
import random
if __name__ == '__main__':
  try:
    from hfo import *
  except:
    print 'Failed to import hfo. To install hfo, in the HFO directory'\
      ' run: \"pip install .\"'
    exit()
  # Connect 4 agents
  agents = []
  for i in xrange(4):
    agents.append(hfo.HFOEnvironment())
    agents[i].connectToAgentServer(6000-i, HFO_Features.HIGH_LEVEL_FEATURE_SET)
  for episode in xrange(5):
    status = HFO_Status.IN_GAME
    while status == HFO_Status.IN_GAME:
      # Grab the state features from the environment from the first agent
      features = agents[0].getState()
      # Take an action and get the current game status
      for agent in agents:
        rand_int = random.randint(0,3)
        if rand_int == 0:
          status = agent.act((HFO_Actions.MOVE, 0, 0))
        elif rand_int == 1:
          status = agent.act((HFO_Actions.SHOOT, 0, 0))
        elif rand_int == 2:
          status = agent.act((HFO_Actions.PASS, 0, 0))
        elif rand_int == 3:
          status = agent.act((HFO_Actions.DRIBBLE, 0, 0))
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
  for agent in agents:
    agent.cleanup()
