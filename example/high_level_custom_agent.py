#!/usr/bin/env python
# encoding: utf-8

#MODIFIED#

# First Start the server: $> bin/start.py
import random, threading, argparse
import itertools
try:
  from hfo import *
except:
  print('Failed to import hfo. To install hfo, in the HFO directory'\
    ' run: \"pip install .\"')
  exit()
params = {'SHT_DST':0.136664020547, 'SHT_ANG':-0.747394386098,
          'PASS_ANG':0.464086704478, 'DRIB_DST':-0.999052871962}

def can_shoot(goal_dist, goal_angle):
  """Returns True if if player can have a good shot at goal"""
  if goal_dist < params['SHT_DST'] and goal_angle > params['SHT_ANG']:
    return True
  else:
    return False

def has_better_pos(dist_to_op, goal_angle, pass_angle, curr_goal_angle):
  """Returns True if teammate is in a better attacking position"""
  if curr_goal_angle > goal_angle or dist_to_op<params['DRIB_DST']:
    return False
  if pass_angle < params['PASS_ANG']:
    return False
  return True

def can_dribble(dist_to_op):
  if dist_to_op > params['DRIB_DST']:
    return True
  else:
    return False

def get_action(state,hfo_env,num_teammates):
  """Returns the action to be taken by the agent"""
  
  goal_dist = float(state[6])
  goal_op_angle = float(state[8])
  if can_shoot(goal_dist, goal_op_angle):
    hfo_env.act(SHOOT)
    return
  for i in range(num_teammates):
    teammate_uniform_number=state[10 + 3*num_teammates + 3*i +2]
    if has_better_pos(dist_to_op = float(state[10 + num_teammates + i]),
                      goal_angle = float(state[10 + i]),
                      pass_angle = float(state[10 + 2*num_teammates + i]),
                      curr_goal_angle = goal_op_angle):
      hfo_env.act(PASS, teammate_uniform_number)
      return
  if can_dribble(dist_to_op = state[9]):
    hfo_env.act(DRIBBLE)
    return
  # If nothing can be done pass
  hfo_env.act(PASS)
    

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000)
  parser.add_argument('--numTeammates', type=int, default=0)
  parser.add_argument('--numOpponents', type=int, default=1)
  args=parser.parse_args()
  hfo_env = HFOEnvironment()
  hfo_env.connectToServer(HIGH_LEVEL_FEATURE_SET,
                      'bin/teams/base/config/formations-dt', args.port,
                      'localhost', 'base_left', False)
  #itertools.count() counts forever
  for episode in itertools.count():
    status=IN_GAME
    count=0
    while status==IN_GAME:
      state = hfo_env.getState()
      #print(state)
      if int(state[5]) == 1: # state[5] is 1 when player has the ball
        tmp = get_action(state,hfo_env,args.numTeammates)  
        #print(tmp)
        #hfo_env.act(tmp)
      else:
        hfo_env.act(MOVE)
      status=hfo_env.step()
      #print(status)
      if status == SERVER_DOWN:
        hfo_env.act(QUIT)
        break 
  

if __name__ == '__main__':
  main()
