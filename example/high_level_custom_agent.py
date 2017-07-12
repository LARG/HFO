#!/usr/bin/env python
# encoding: utf-8

#MODIFIED#

# First Start the server: $> bin/start.py
import argparse
import random
try:
  import hfo
except ImportError:
  print('Failed to import hfo. To install hfo, in the HFO directory'\
    ' run: \"pip install .\"')
  exit()
params = {'SHT_DST':0.136664020547, 'SHT_ANG':-0.747394386098,
          'PASS_ANG':0.464086704478, 'DRIB_DST':-0.999052871962}

def can_shoot(goal_dist, goal_angle):
  """Returns True if if player can have a good shot at goal"""
  return bool((goal_dist < params['SHT_DST']) and (goal_angle > params['SHT_ANG']))

def has_better_pos(dist_to_op, goal_angle, pass_angle, curr_goal_angle):
  """Returns True if teammate is in a better attacking position"""
  if (curr_goal_angle > goal_angle) or (dist_to_op < params['DRIB_DST']):
    return False
  if pass_angle < params['PASS_ANG']:
    return False
  return True

def can_dribble(dist_to_op):
  return bool(dist_to_op > params['DRIB_DST'])

def get_action(state,hfo_env,num_teammates,rand_pass):
  """Decides and performs the action to be taken by the agent."""
  
  goal_dist = float(state[6])
  goal_op_angle = float(state[8])
  if can_shoot(goal_dist, goal_op_angle):
    hfo_env.act(hfo.SHOOT)
    return
  if rand_pass and (num_teammates > 1):
    team_list = random.shuffle(range(num_teammates))
  else:
    team_list = range(num_teammates)
  for i in team_list:
    teammate_uniform_number=state[10 + 3*num_teammates + 3*i +2]
    if has_better_pos(dist_to_op = float(state[10 + num_teammates + i]),
                      goal_angle = float(state[10 + i]),
                      pass_angle = float(state[10 + 2*num_teammates + i]),
                      curr_goal_angle = goal_op_angle):
      hfo_env.act(hfo.PASS, teammate_uniform_number)
      return
  # not sure if below check is needed - doDribble in agent.cpp includes
  # (via doPreprocess) doForceKick, which may cover this situation depending
  # on what existKickableOpponent returns.
  if can_dribble(dist_to_op = state[9]):
    hfo_env.act(hfo.DRIBBLE)
  else:
    # If nothing can be done, do not do anything
    hfo_env.act(hfo.NOOP)
  return
    

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000)
  parser.add_argument('--seed', type=int, default=None)
  parser.add_argument('--rand-pass', type=bool, default=False)
  parser.add_argument('--eps', type=float, default=0)
  parser.add_argument('--numTeammates', type=int, default=0)
  parser.add_argument('--numOpponents', type=int, default=1)
  args=parser.parse_args()
  if args.seed:
    random.seed(args.seed)
  hfo_env = hfo.HFOEnvironment()
  hfo_env.connectToServer(hfo.HIGH_LEVEL_FEATURE_SET,
                          'bin/teams/base/config/formations-dt', args.port,
                          'localhost', 'base_left', False)
  status=hfo.IN_GAME
  while status != hfo.SERVER_DOWN:
    state = hfo_env.getState()
    #print(state)
    if int(state[5]) == 1: # state[5] is 1 when player has the ball
      if (args.eps > 0) and (random.random() < args.eps):
        if random.random() < 0.5:
          hfo_env.act(hfo.SHOOT)
        else:
          hfo_env.act(hfo.DRIBBLE)
      else:
        get_action(state,hfo_env,args.numTeammates,args.rand_pass)
    else:
      hfo_env.act(hfo.MOVE)
    status=hfo_env.step()
    #print(status)

  hfo_env.act(hfo.QUIT)
  exit()
  

if __name__ == '__main__':
  main()
