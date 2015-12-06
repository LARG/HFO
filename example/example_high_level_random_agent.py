#!/usr/bin/env python
# encoding: utf-8

# First Start the server: $> bin/start.py
import random, threading, argparse
try:
  from hfo import *
except:
  print 'Failed to import hfo. To install hfo, in the HFO directory'\
    ' run: \"pip install .\"'
  exit()

def get_random_action():
  """Returns a random high-level action. Pass is omitted for simplicity."""
  high_lv_actions = [HFO_Actions.SHOOT, HFO_Actions.DRIBBLE]
  return random.choice(high_lv_actions)

def play_hfo(num):
  """ Method called by a thread to play 5 games of HFO """
  hfo_env = hfo.HFOEnvironment()
  hfo_env.connectToAgentServer(6000 + num, HFO_Features.HIGH_LEVEL_FEATURE_SET)
  try:
    for episode in xrange(5):
      status = HFO_Status.IN_GAME
      while status == HFO_Status.IN_GAME:
        state = hfo_env.getState()
        if state[5] == 1: #state[5]  is 1 when player has the ball
          hfo_env.act(get_random_action())
        else:
          hfo_env.act(HFO_Actions.MOVE)
        status = hfo_env.step()
  except:
    pass
  finally:
    print "Agent " + str(num) + " exiting."
    hfo_env.cleanup()

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('num_agents', type=int, help='Number of agents to start. '\
                      'NOTE: server must be started with this number of agents.')
  args = parser.parse_args()
  for i in xrange(args.num_agents):
    t = threading.Thread(target=play_hfo, args=(i,))
    t.start()

if __name__ == '__main__':
  main()
