#!/usr/bin/env python
from __future__ import print_function
# encoding: utf-8

#MODIFIED#

# First Start the server: $> bin/start.py
import argparse
import itertools
import math
import random
try:
  import hfo
except ImportError:
  print('Failed to import hfo. To install hfo, in the HFO directory'\
    ' run: \"pip install .\"')
  exit()
params = {'SHT_DST':0.136664020547, 'SHT_ANG':-0.747394386098,
          'PASS_ANG':0.464086704478, 'DRIB_DST':-0.999052871962}

HALF_FIELD_WIDTH = 68 # y coordinate -34 to 34 (-34 = bottom 34 = top)
HALF_FIELD_LENGTH = 52.5 # x coordinate 0 to 52.5 (0 = goalline 52.5 = center)
oldHFODist = math.sqrt(math.pow(HALF_FIELD_WIDTH,2)+math.pow(HALF_FIELD_LENGTH,2))
newHFODist = math.sqrt(math.pow((HALF_FIELD_WIDTH*2),2)+math.pow(HALF_FIELD_LENGTH,2))

def correct_dist(curr_dist):
  old_dist = min(1.0,(((curr_dist+1)*(newHFODist/oldHFODist))-1))
  return old_dist

def can_shoot(goal_dist, goal_angle):
  """Returns True if if player may have a good shot at the goal"""
  return bool((correct_dist(goal_dist) < params['SHT_DST']) and (goal_angle > params['SHT_ANG']))

def has_better_pos(dist_to_op, goal_angle, pass_angle, curr_goal_angle):
  """Returns True if teammate is in a better attacking position"""
  if (curr_goal_angle > goal_angle) or (correct_dist(dist_to_op) < params['DRIB_DST']):
    return False
  if pass_angle < params['PASS_ANG']:
    return False
  return True

def get_action(state,hfo_env,num_teammates,rand_pass):
  """Decides and performs the action to be taken by the agent."""
  
  goal_dist = float(state[6])
  goal_op_angle = float(state[8])
  if can_shoot(goal_dist, goal_op_angle):
    hfo_env.act(hfo.SHOOT)
    return
  team_list = list(range(num_teammates))
  if rand_pass and (num_teammates > 1):
    random.shuffle(team_list)
  for i in team_list:
    teammate_uniform_number=state[10 + 3*num_teammates + 3*i +2]
    if has_better_pos(dist_to_op = float(state[10 + num_teammates + i]),
                      goal_angle = float(state[10 + i]),
                      pass_angle = float(state[10 + 2*num_teammates + i]),
                      curr_goal_angle = goal_op_angle):
      hfo_env.act(hfo.PASS, teammate_uniform_number)
      return
  # no check for can_dribble is needed; doDribble in agent.cpp includes
  # (via doPreprocess) doForceKick, which will cover this situation since
  # existKickableOpponent is based on distance.
  hfo_env.act(hfo.DRIBBLE)
  return
    

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000, help="Server port")
  parser.add_argument('--seed', type=int, default=None,
                      help="Python randomization seed; uses python default if 0 or not given")
  parser.add_argument('--rand-pass', action="store_true",
                      help="Randomize order of checking teammates for a possible pass")
  parser.add_argument('--epsilon', type=float, default=0,
                      help="Probability of a random action if has the ball, to adjust difficulty")
  parser.add_argument('--record', action='store_true',
                      help="If doing HFO --record")
  parser.add_argument('--rdir', type=str, default='log/',
                      help="Set directory to use if doing --record")
  args=parser.parse_args()
  if args.seed:
    random.seed(args.seed)
  hfo_env = hfo.HFOEnvironment()
  if args.record:
    hfo_env.connectToServer(hfo.HIGH_LEVEL_FEATURE_SET,
                            'bin/teams/base/config/formations-dt', args.port,
                            'localhost', 'base_left', False,
                            record_dir=args.rdir)
  else:
    hfo_env.connectToServer(hfo.HIGH_LEVEL_FEATURE_SET,
                            'bin/teams/base/config/formations-dt', args.port,
                            'localhost', 'base_left', False)
  num_teammates = hfo_env.getNumTeammates()
  #num_opponents = hfo_env.getNumOpponents()
  if args.seed:
    if (args.rand_pass and (num_teammates > 1)) or (args.epsilon > 0):
      print("Python randomization seed: {0:d}".format(args.seed))
    else:
      print("Python randomization seed useless without --rand-pass w/2+ teammates or --epsilon >0")
  if args.rand_pass and (num_teammates > 1):
    print("Randomizing order of checking for a pass")
  if args.epsilon > 0:
    print("Using epsilon {0:n}".format(args.epsilon))
  for episode in itertools.count():
    num_eps = 0
    num_had_ball = 0
    num_move = 0
    status = hfo.IN_GAME
    while status == hfo.IN_GAME:
      state = hfo_env.getState()
      #print(state)
      if int(state[5]) == 1: # state[5] is 1 when player has the ball
        if (args.epsilon > 0) and (random.random() < args.epsilon):
          if random.random() < 0.5:
            hfo_env.act(hfo.SHOOT)
          else:
            hfo_env.act(hfo.DRIBBLE)
          num_eps += 1
        else:
          get_action(state,hfo_env,num_teammates,args.rand_pass)
        num_had_ball += 1
      else:
        hfo_env.act(hfo.MOVE)
        num_move += 1
      status=hfo_env.step()
      #print(status)

    # Check the outcome of the episode
    print("Episode {0:n} ended with {1:s}".format(episode,
                                                  hfo_env.statusToString(status)))
    if args.epsilon > 0:
      print("\tNum move: {0:n}; Random action: {1:n}; Nonrandom: {2:n}".format(num_move,
                                                                               num_eps,
                                                                               (num_had_ball-
                                                                                num_eps)))
    # Quit if the server goes down
    if status == hfo.SERVER_DOWN:
      hfo_env.act(hfo.QUIT)
      exit()
      

if __name__ == '__main__':
  main()
