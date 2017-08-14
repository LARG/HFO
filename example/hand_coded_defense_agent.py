#!/usr/bin/env python
"""
This is a hand-coded defense agent, using hand_coded_defense_agent.cpp as a starting point,
that should be able to play, for instance, a 2v2 game againt 2 offense npcs. It requires a goal
keeper/goalie.
"""
from __future__ import print_function
# encoding: utf-8

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

GOAL_POS_X = 0.9
GOAL_POS_Y = 0.0

# below - from hand_coded_defense_agent.cpp except LOW_KICK_DIST
HALF_FIELD_WIDTH = 68 # y coordinate -34 to 34 (-34 = bottom 34 = top)
HALF_FIELD_LENGTH = 52.5 # x coordinate 0 to 52.5 (0 = goalline 52.5 = center)
params = {'KICK_DIST':(1.504052352*1), 'OPEN_AREA_HIGH_LIMIT_X':0.747311440447,
          'TACKLE_DIST':(1.613456553*1), 'LOW_KICK_DIST':((5*5)/HALF_FIELD_LENGTH)}


def get_dist_normalized(ref_x, ref_y, src_x, src_y):
  return math.sqrt(math.pow((ref_x - src_x),2) +
                   math.pow(((HALF_FIELD_WIDTH/HALF_FIELD_LENGTH)*(ref_y - src_y)),2))

##def is_kickable(ball_pos_x, ball_pos_y, kicker_pos_x, kicker_pos_y):
##  return get_dist_normalized(ball_pos_x, ball_pos_y,
##                             kicker_pos_x, kicker_pos_y) < params['KICK_DIST']

def is_tackleable(agent_pos_x, agent_pos_y, ball_dist, opp_pos_x, opp_pos_y):
  return (get_dist_normalized(agent_pos_x,
                              agent_pos_y,
                              opp_pos_x,
                              opp_pos_y) < params['TACKLE_DIST']) and (ball_dist <
                                                                       params['LOW_KICK_DIST'])

def ball_moving_toward_goal(ball_pos_x, ball_pos_y, old_ball_pos_x, old_ball_pos_y):
  return (get_dist_normalized(ball_pos_x, ball_pos_y,
                              GOAL_POS_X, GOAL_POS_Y) < min(params['KICK_DIST'],
                                                            get_dist_normalized(old_ball_pos_x,
                                                                                old_ball_pos_y,
                                                                                GOAL_POS_X,
                                                                                GOAL_POS_Y)))

def ball_nearer_to_goal(ball_pos_x, ball_pos_y, agent_pos_x, agent_pos_y):
  return get_dist_normalized(ball_pos_x, ball_pos_y,
                             GOAL_POS_X, GOAL_POS_Y) < min(params['KICK_DIST'],
                                                           get_dist_normalized(agent_pos_x,
                                                                               agent_pos_y,
                                                                               GOAL_POS_X,
                                                                               GOAL_POS_Y))

def get_sorted_opponents(state_vec, num_opponents, num_teammates, pos_x, pos_y):
  """
  Returns a list of tuple(unum, dist, opp_pos_x, opp_pos_y),
  sorted in increasing order of dist from the given position
  """
  unum_list = []
  for i in range(num_opponents):
    unum = state_vec[9+(i*3)+(6*num_teammates)+3]
    if unum > 0:
      opp_pos_x = state_vec[9+(i*3)+(6*num_teammates)+1]
      opp_pos_y = state_vec[9+(i*3)+(6*num_teammates)+2]
      dist = get_dist_normalized(pos_x, pos_y, opp_pos_x, opp_pos_y)
      unum_list.append(tuple([unum, dist, opp_pos_x, opp_pos_y]))
    # otherwise, unknown
  if len(unum_list) > 1:
    return sorted(unum_list, key=lambda x: x[1])
  return unum_list

def is_in_open_area(pos_x, ignored_pos_y):
  return pos_x >= params['OPEN_AREA_HIGH_LIMIT_X']

def add_num_times(action, main_dict, opt_dict=None):
  main_dict[action] += 1
  if opt_dict:
    opt_dict[action] += 1
  return action

def do_defense_action(state_vec, hfo_env,
                      num_opponents, num_teammates,
                      old_ball_pos_x, old_ball_pos_y,
                      num_times_overall, num_times_kickable,
                      misc_tracked):
  """Figures out and does the (hopefully) best defense action."""
  min_vec_size = 10 + (6*num_teammates) + (3*num_opponents)
  if (len(state_vec) < min_vec_size):
    raise LookupError("Feature vector length is {0:d} not {1:d}".format(len(state_vec),
                                                                        min_vec_size))
  agent_pos_x = state_vec[0]
  agent_pos_y = state_vec[1]
  ball_pos_x = state_vec[3]
  ball_pos_y = state_vec[4]

  # if get high_level working for invalid
  if (min(agent_pos_x,agent_pos_y,ball_pos_x,ball_pos_y) < -1):
    hfo_env.act(add_num_times(hfo.REORIENT,num_times_overall))
    return

  ball_toward_goal = ball_moving_toward_goal(ball_pos_x, ball_pos_y,
                                             old_ball_pos_x, old_ball_pos_y)

  ball_nearer_goal = ball_nearer_to_goal(ball_pos_x, ball_pos_y,
                                         agent_pos_x, agent_pos_y)

  ball_sorted_list = get_sorted_opponents(state_vec, num_opponents, num_teammates,
                                          pos_x=ball_pos_x, pos_y=ball_pos_y)
  if not ball_sorted_list: # unknown opponent positions/unums
    print("No known opponent locations (btg {0!r}; bng {1!r}; ".format(ball_toward_goal,
                                                                       ball_nearer_goal) +
          "ball xy {0:n}, {1:n}; ball old xy {2:n}, {3:n}; kickable {4:n})".format(ball_pos_x,
                                                                                   ball_pos_y,
                                                                                   old_ball_pos_x,
                                                                                   old_ball_pos_y,
                                                                                   state_vec[5]))
    if ((min(agent_pos_x,agent_pos_y,ball_pos_x,ball_pos_y) <= -1) or
        (max(agent_pos_x,agent_pos_y,ball_pos_x,ball_pos_y) >= 1)):
      # remove if get high-level working for invalid
      hfo_env.act(add_num_times(hfo.REORIENT,num_times_overall))
    elif ball_toward_goal:
      if ball_nearer_goal or (state_vec[5] > 0):
        hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
      else:
        hfo_env.act(add_num_times(hfo.INTERCEPT,num_times_overall))
    else:
      hfo_env.act(add_num_times(hfo.MOVE,num_times_overall))
    return

  goal_sorted_list = get_sorted_opponents(state_vec, num_opponents, num_teammates,
                                          pos_x=GOAL_POS_X, pos_y=GOAL_POS_Y)

  if ball_toward_goal:
    if ball_sorted_list[0][1] < params['LOW_KICK_DIST']:
      ball_toward_goal = False
    elif goal_sorted_list[0][1] < get_dist_normalized(ball_pos_x,ball_pos_y,
                                                      GOAL_POS_X,GOAL_POS_Y):
      ball_toward_goal = False

  is_tackleable_opp = is_tackleable(agent_pos_x, agent_pos_y,
                                    ball_sorted_list[0][1],
                                    ball_sorted_list[0][2], ball_sorted_list[0][3])

  agent_to_ball_dist = get_dist_normalized(agent_pos_x, agent_pos_y,
                                           ball_pos_x, ball_pos_y)

  if state_vec[5] > 0: # kickable distance of player
    misc_tracked['max_kickable_dist'] = max(agent_to_ball_dist,misc_tracked['max_kickable_dist'])
    if is_tackleable_opp:
      hfo_env.act(add_num_times(hfo.MOVE,num_times_overall,num_times_kickable)) # will do tackle
    else:
      hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall,num_times_kickable))
    return

  if goal_sorted_list[0][0] != ball_sorted_list[0][0]:
    if is_in_open_area(ball_sorted_list[0][2],
                       ball_sorted_list[0][3]) and is_in_open_area(goal_sorted_list[0][2],
                                                                   goal_sorted_list[0][3]):
      if ball_sorted_list[0][1] < params['LOW_KICK_DIST']:
        hfo_env.act(add_num_times(hfo.MARK_PLAYER,num_times_overall),
                    goal_sorted_list[0][0])
      elif agent_to_ball_dist < ball_sorted_list[0][1]:
        if ball_nearer_goal:
          hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
        elif ball_toward_goal:
          hfo_env.act(add_num_times(hfo.INTERCEPT,num_times_overall))
        else:
          hfo_env.act(add_num_times(hfo.GO_TO_BALL,num_times_overall))
      else:
        hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
    elif ball_sorted_list[0][1] >= params['KICK_DIST']:
      if agent_to_ball_dist < ball_sorted_list[0][1]:
        if ball_nearer_goal:
          hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
        elif ball_toward_goal:
          hfo_env.act(add_num_times(hfo.INTERCEPT,num_times_overall))
        else:
          hfo_env.act(add_num_times(hfo.GO_TO_BALL,num_times_overall))
      else:
        hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
    elif is_tackleable_opp and (not is_in_open_area(ball_sorted_list[0][2],
                                                    ball_sorted_list[0][3])):
      hfo_env.act(add_num_times(hfo.MOVE,num_times_overall))
    elif ball_sorted_list[0][1] < (1*params['LOW_KICK_DIST']):
      hfo_env.act(add_num_times(hfo.MARK_PLAYER,num_times_overall),
                  goal_sorted_list[0][0])
    else:
      hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
    return

  if is_in_open_area(ball_sorted_list[0][2],ball_sorted_list[0][3]):
    if ball_sorted_list[0][1] < params['KICK_DIST']:
      hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
    elif agent_to_ball_dist < params['KICK_DIST']:
      if ball_nearer_goal:
        hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
      elif ball_toward_goal:
        hfo_env.act(add_num_times(hfo.INTERCEPT,num_times_overall))
      else:
        hfo_env.act(add_num_times(hfo.GO_TO_BALL,num_times_overall))
    elif is_tackleable_opp:
      hfo_env.act(add_num_times(hfo.MOVE,num_times_overall))
    else:
      hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
  else:
    if ball_sorted_list[0][1] >= max(params['KICK_DIST'],agent_to_ball_dist):
      if ball_nearer_goal:
        hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
      elif ball_toward_goal:
        hfo_env.act(add_num_times(hfo.INTERCEPT,num_times_overall))
      else:
        hfo_env.act(add_num_times(hfo.GO_TO_BALL,num_times_overall))
    elif ball_sorted_list[0][1] >= params['KICK_DIST']:
      hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
    elif is_tackleable_opp:
      hfo_env.act(add_num_times(hfo.MOVE,num_times_overall))
    else:
      hfo_env.act(add_num_times(hfo.REDUCE_ANGLE_TO_GOAL,num_times_overall))
  return

def do_random_defense_action(state, hfo_env):
  if state[5] > 0: # kickable
    hfo_env.act(hfo.MOVE)
  else:
    if random.random() < 0.25:
      hfo_env.act(hfo.REDUCE_ANGLE_TO_GOAL)
    else:
      hfo_env.act(hfo.MOVE)

  return

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000, help="Server port")
  parser.add_argument('--seed', type=int, default=None,
                      help="Python randomization seed; uses python default if 0 or not given")
  parser.add_argument('--epsilon', type=float, default=0,
                      help="Probability of a random action, to adjust difficulty")
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
                            'localhost', 'base_right', play_goalie=False,
                            record_dir=args.rdir)
  else:
    hfo_env.connectToServer(hfo.HIGH_LEVEL_FEATURE_SET,
                            'bin/teams/base/config/formations-dt', args.port,
                            'localhost', 'base_right', play_goalie=False)
  numTeammates = hfo_env.getNumTeammates()
  numOpponents = hfo_env.getNumOpponents()
  if args.seed:
    if args.epsilon > 0:
      print("Python randomization seed: {0:d}".format(args.seed))
    else:
      print("Python randomization seed useless without --epsilon >0")
  if args.epsilon > 0:
    print("Using epsilon {0:n}".format(args.epsilon))
  my_unum = hfo_env.getUnum()
  assert ((my_unum > 1) and (my_unum <= 11)), "Bad unum {!r}".format(my_unum)
  print("My unum is {0:d}".format(my_unum))
  num_times_overall = {}
  num_times_kickable = {}
  for action in range(hfo.NUM_HFO_ACTIONS):
    num_times_overall[action] = 0
    num_times_kickable[action] = 0
  misc_tracked = {'max_kickable_dist':0}
  for episode in itertools.count():
    old_ball_pos_x = -1
    old_ball_pos_y = 0
    episode_start = True
    status = hfo.IN_GAME
    while status == hfo.IN_GAME:
      state = hfo_env.getState()
      if episode_start:
        if (state[3] >= -1) and (state[3] <= 1):
          old_ball_pos_x = state[3]
        if (state[4] >= -1) and (state[4] <= 1):
          old_ball_pos_y = state[4]
        episode_start = False
      if (args.epsilon > 0) and (random.random() < args.epsilon):
        do_random_defense_action(state, hfo_env)
      else:
        do_defense_action(state_vec=state, hfo_env=hfo_env,
                          num_opponents=numOpponents, num_teammates=numTeammates,
                          old_ball_pos_x=old_ball_pos_x, old_ball_pos_y=old_ball_pos_y,
                          num_times_overall=num_times_overall,
                          num_times_kickable=num_times_kickable,
                          misc_tracked=misc_tracked)
      old_ball_pos_x=state[3]
      old_ball_pos_y=state[4]
      status=hfo_env.step()
      #print(status)

    # Quit if the server goes down
    if status == hfo.SERVER_DOWN:
      for action in range(hfo.NUM_HFO_ACTIONS):
        if num_times_overall[action]:
          print("Overall times {0!s}: {1:d}".format(hfo_env.actionToString(action),
                                                    num_times_overall[action]))
      for action in range(hfo.NUM_HFO_ACTIONS):
        if num_times_kickable[action]:
          print("Kickable times {0!s}: {1:d}".format(hfo_env.actionToString(action),
                                                     num_times_kickable[action]))
      print("Max kickable dist: {0:n}".format(misc_tracked['max_kickable_dist']))
      hfo_env.act(hfo.QUIT)
      exit()

    # Check the outcome of the episode
    print("Episode {0:d} ended with {1:s}".format(episode,
                                                  hfo_env.statusToString(status)))

if __name__ == '__main__':
  main()
