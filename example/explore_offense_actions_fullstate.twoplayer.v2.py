#!/usr/bin/env python
"""
This is a script to explore what actions are available under what circumstances,
plus testing out feedback and the use of two players controlled by the same script
but two (coordinated) processes.
"""
from __future__ import print_function
# encoding: utf-8

import argparse
#import functools
import itertools
import math
import multiprocessing
import multiprocessing.sharedctypes
import os
import random
import struct
import subprocess
import sys
import time
import warnings

try:
  import hfo
except ImportError:
  print('Failed to import hfo. To install hfo, in the HFO directory'\
    ' run: \"pip install .\"')
  exit()

HALF_FIELD_WIDTH = 68 # y coordinate
HALF_FIELD_FULL_WIDTH = HALF_FIELD_WIDTH * 1.2
HALF_FIELD_LENGTH = 52.5 # x coordinate
HALF_FIELD_FULL_LENGTH = HALF_FIELD_LENGTH * 1.2
GOAL_WIDTH = 14.02
MAX_RADIUS = math.sqrt(((HALF_FIELD_WIDTH/2)**2) + (HALF_FIELD_LENGTH**2)) # not actually correct, but works...
ERROR_TOLERANCE = math.pow(sys.float_info.epsilon,0.25)
POS_ERROR_TOLERANCE = 0.05

MAX_REAL_X_VALID = 1.1*HALF_FIELD_LENGTH
MIN_REAL_X_VALID = -0.1*HALF_FIELD_LENGTH
MAX_REAL_Y_VALID = 1.1*(HALF_FIELD_WIDTH/2)
MIN_REAL_Y_VALID = -1.1*(HALF_FIELD_WIDTH/2)

def unnormalize(pos, min_val, max_val, silent=False):
  assert max_val > min_val
  if (not silent) and (abs(pos) > 1.0+ERROR_TOLERANCE):
    print("Pos {0:n} fed to unnormalize (min_val {1:n}, max_val {2:n})".format(
      pos, min_val, max_val), file=sys.stderr)
    sys.stderr.flush()
  pos = min(1.0,max(-1.0,pos))
  top = (pos - -1.0)/(1 - -1.0)
  bot = max_val - min_val
  return (top*bot) + min_val

def get_y_unnormalized(y_pos, silent=False):
  y_pos_real = unnormalize(y_pos, MIN_REAL_Y_VALID, MAX_REAL_Y_VALID, silent=silent)
  est_y_pos = get_y_normalized(y_pos_real, silent=silent)
  if abs(y_pos - est_y_pos) > POS_ERROR_TOLERANCE:
    raise RuntimeError(
      "Bad denormalization/normalization of {0:n} to {1:n}; reverse {2:n}".format(
        y_pos, y_pos_real, est_y_pos))
  return y_pos_real

def get_x_unnormalized(x_pos, silent=False):
  x_pos_real = unnormalize(x_pos, MIN_REAL_X_VALID, MAX_REAL_X_VALID, silent=silent)
  est_x_pos = get_x_normalized(x_pos_real, silent=silent)
  if abs(x_pos - est_x_pos) > POS_ERROR_TOLERANCE:
    raise RuntimeError(
      "Bad denormalization/normalization of {0:n} to {1:n}; reverse {2:n}".format(
        x_pos, x_pos_real, est_x_pos))
  return x_pos_real

def normalize(pos, min_val, max_val, silent=False):
  assert max_val > min_val
  top = (pos - min_val)/(max_val - min_val)
  bot = 1.0 - -1.0
  num = (top*bot) + -1.0
  if abs(num) > 1.0+POS_ERROR_TOLERANCE:
    if abs(num) > 1.1:
      raise RuntimeError("Pos {0:n} gives normalized num {1:n} (min_val {2:n}, max_val {3:n})".format(
        pos, num, min_val, max_val))
    elif not silent:
      print(
        "Pos {0:n} gives normalized num {1:n} (min_val {2:n}, max_val {3:n})".format(
          pos, num, min_val, max_val), file=sys.stderr)
      sys.stderr.flush()
  return min(1.0,max(-1.0,num))

def get_y_normalized(y_pos, silent=False):
  y_pos_norm = normalize(y_pos, MIN_REAL_Y_VALID, MAX_REAL_Y_VALID, silent=silent)
##  est_y_pos = get_y_unnormalized(y_pos_norm, silent=silent)
##  if abs(y_pos - est_y_pos) > POS_ERROR_TOLERANCE:
##    raise RuntimeError("Bad normalization/denormalization of {0:n} to {1:n}; reverse {2:n}".format(
##      y_pos, y_pos_norm, est_y_pos))
  return y_pos_norm

def get_x_normalized(x_pos, silent=False):
  return normalize(x_pos, MIN_REAL_X_VALID, MAX_REAL_X_VALID, silent=silent)

GOAL_POS_X = get_x_normalized(HALF_FIELD_LENGTH)
GOAL_TOP_POS_Y = get_y_normalized(-1*(GOAL_WIDTH/2))
GOAL_BOTTOM_POS_Y = get_y_normalized(GOAL_WIDTH/2)

MAX_POS_Y_BALL_SAFE = get_y_normalized(HALF_FIELD_WIDTH/2) - POS_ERROR_TOLERANCE
MIN_POS_Y_BALL_SAFE = get_y_normalized(-0.5*HALF_FIELD_WIDTH) + POS_ERROR_TOLERANCE
MAX_POS_X_BALL_SAFE = get_x_normalized(HALF_FIELD_LENGTH) - POS_ERROR_TOLERANCE
MIN_POS_X_BALL_SAFE = get_x_normalized(0) + POS_ERROR_TOLERANCE

MAX_POS_X_OK = 1.0 - ERROR_TOLERANCE
MIN_POS_X_OK = -1.0 + ERROR_TOLERANCE
MAX_POS_Y_OK = MAX_POS_X_OK
MIN_POS_Y_OK = MIN_POS_X_OK

def get_dist_real(ref_x, ref_y, src_x, src_y, silent=False):
  ref_x_real = get_x_unnormalized(ref_x, silent=silent)
  ref_y_real = get_y_unnormalized(ref_y, silent=silent)
  src_x_real = get_x_unnormalized(src_x, silent=silent)
  src_y_real = get_y_unnormalized(src_y, silent=silent)

  return math.sqrt(math.pow((ref_x_real - src_x_real),2) +
                   math.pow((ref_y_real - src_y_real),2))

def get_dist_from_real(ref_x_real, ref_y_real, src_x_real, src_y_real):
    return math.sqrt(math.pow((ref_x_real - src_x_real),2) +
                     math.pow((ref_y_real - src_y_real),2))

def get_dist_normalized(ref_x, ref_y, src_x, src_y):
  return math.sqrt(math.pow((ref_x - src_x),2) +
                   math.pow(((HALF_FIELD_WIDTH/HALF_FIELD_LENGTH)*(ref_y - src_y)),2))

#Instead of adding six as a dependency, this code was copied from the six implementation.
#six is Copyright (c) 2010-2015 Benjamin Peterson

if sys.version_info[0] >= 3:
    def iterkeys(d, **kw):
        return iter(d.keys(**kw))

    def iteritems(d, **kw):
        return iter(d.items(**kw))

    def itervalues(d, **kw):
        return iter(d.values(**kw))
else:
    def iterkeys(d, **kw):
        return iter(d.iterkeys(**kw))

    def iteritems(d, **kw):
        return iter(d.iteritems(**kw))

    def itervalues(d, **kw):
        return iter(d.itervalues(**kw))

INTENT_BALL_COLLISION = 0
INTENT_BALL_KICKABLE = 1
INTENT_GOAL_COLLISION = 2
INTENT_PLAYER_COLLISION = 3

INTENT_DICT = {INTENT_BALL_COLLISION: 'INTENT_BALL_COLLISION',
               INTENT_BALL_KICKABLE: 'INTENT_BALL_KICKABLE',
               INTENT_GOAL_COLLISION: 'INTENT_GOAL_COLLISION',
               INTENT_PLAYER_COLLISION: 'INTENT_PLAYER_COLLISION'}

BIT_LIST_LEN = 8
STRUCT_PACK = "BH"

def get_dist_from_proximity(proximity, max_dist=MAX_RADIUS):
  proximity_real = unnormalize(proximity, 0.0, 1.0)
  dist = (1 - proximity_real)*max_dist
  if (dist > (max_dist+ERROR_TOLERANCE)) or (dist <= (-1.0*ERROR_TOLERANCE)):
    warnings.warn("Proximity {0:n} gives dist {1:n} (max_dist {2:n})".format(proximity,dist,max_dist))
  return min(max_dist,max(0,dist))

def get_proximity_from_dist(dist, max_dist=MAX_RADIUS):
  if dist > (max_dist+ERROR_TOLERANCE):
    print("Dist {0:n} is above max_dist {1:n}".format(dist, max_dist), file=sys.stderr)
    sys.stderr.flush()
  proximity_real = min(1.0, max(0.0, (1.0 - (dist/max_dist))))
  return normalize(proximity_real, 0.0, 1.0)

# MAX_GOAL_DIST - may wish to work out...

def get_angle(sin_angle,cos_angle):
  if max(abs(sin_angle),abs(cos_angle)) <= sys.float_info.epsilon:
    warnings.warn(
      "Unable to accurately determine angle from sin_angle {0:n} cos_angle {1:n}".format(
        sin_angle, cos_angle))
    return None
  angle = math.degrees(math.atan2(sin_angle,cos_angle))
  if angle < 0:
    angle += 360
  return angle

def get_abs_angle(body_angle,rel_angle):
  if (body_angle is None) or (rel_angle is None):
    return None
  angle = body_angle + rel_angle
  while angle >= 360:
    angle -= 360
  if angle < 0:
    if abs(angle) <= ERROR_TOLERANCE:
      return 0.0
    warnings.warn("Bad body_angle {0:n} and/or rel_angle {1:n}".format(body_angle,rel_angle))
    return None
  return angle

def get_angle_diff(angle1, angle2):
  if angle1 > angle2:
    return min((angle1 - angle2),abs(angle1 - angle2 - 360))
  elif angle1 < angle2:
    return min((angle2 - angle1),abs(angle2 - angle1 - 360))

  return 0.0

def reverse_angle(angle):
  angle += 180
  while angle >= 360:
    angle -= 360
  return angle

def get_abs_x_y_pos(abs_angle, dist, self_x_pos, self_y_pos, warn=True, of_what=""):
  poss_xy_pos_real = {}
  max_deviation_xy_pos_real = {}
  total_deviation_xy_pos_real = {}
  dist_xy_pos_real = {}
  self_x_pos_real = get_x_unnormalized(self_x_pos)
  self_y_pos_real = get_y_unnormalized(self_y_pos)

  start_string = ""
  if of_what:
    start_string = of_what + ': '

  for angle in [(abs_angle-1),(abs_angle+1),abs_angle]:
    angle_radians = math.radians(angle)
    sin_angle = math.sin(angle_radians)
    cos_angle = math.cos(angle_radians)


    est_x_pos_real = (cos_angle*dist) + self_x_pos_real
    est_y_pos_real = (sin_angle*dist) + self_y_pos_real
    if ((MIN_REAL_X_VALID*(1-POS_ERROR_TOLERANCE))
        <= est_x_pos_real <=
        (MAX_REAL_X_VALID*(1+POS_ERROR_TOLERANCE))) and ((MIN_REAL_Y_VALID*(1-POS_ERROR_TOLERANCE))
                                                         <= est_y_pos_real <=
                                                         (MAX_REAL_Y_VALID*(1+POS_ERROR_TOLERANCE))):
      poss_xy_pos_real[angle] = (est_x_pos_real, est_y_pos_real)

      if est_x_pos_real < MIN_REAL_X_VALID:
        x_deviation = (MIN_REAL_X_VALID-est_x_pos_real)/(MAX_REAL_X_VALID-MIN_REAL_X_VALID)
      elif est_x_pos_real > MAX_REAL_X_VALID:
        x_deviation = (est_x_pos_real-MAX_REAL_X_VALID)/(MAX_REAL_X_VALID-MIN_REAL_X_VALID)
      else:
        x_deviation = 0.0

      if est_y_pos_real < MIN_REAL_Y_VALID:
        y_deviation = (MIN_REAL_Y_VALID-est_y_pos_real)/(MAX_REAL_Y_VALID-MIN_REAL_Y_VALID)
      elif est_y_pos_real > MAX_REAL_Y_VALID:
        y_deviation = (est_y_pos_real-MAX_REAL_Y_VALID)/(MAX_REAL_Y_VALID-MIN_REAL_Y_VALID)
      else:
        y_deviation = 0.0

      max_deviation_xy_pos_real[angle] = max(x_deviation,y_deviation)
      total_deviation_xy_pos_real[angle] = x_deviation+y_deviation

      dist_xy_pos_real[angle] = get_dist_from_real(est_x_pos_real,
                                                   est_y_pos_real,
                                                   min(MAX_REAL_X_VALID,
                                                       max(MIN_REAL_X_VALID,
                                                           est_x_pos_real)),
                                                   min(MAX_REAL_Y_VALID,
                                                       max(MIN_REAL_Y_VALID,
                                                           est_y_pos_real)))

    elif (angle == abs_angle) and (not poss_xy_pos_real):
      error_strings = []
      if not ((MIN_REAL_X_VALID*(1-POS_ERROR_TOLERANCE))
              <= est_x_pos_real <=
              (MAX_REAL_X_VALID*(1+POS_ERROR_TOLERANCE))):
        error_strings.append(
          "{0!s}Bad est_x_pos_real {1:n} from self_x_pos_real {2:n} (self_x_pos {3:n}), angle {4:n} ({5:n} {6:n}), dist {7:n}".format(
            start_string,est_x_pos_real, self_x_pos_real, self_x_pos, abs_angle, sin_angle, cos_angle, dist))
      if not ((MIN_REAL_Y_VALID*(1-POS_ERROR_TOLERANCE))
              <= est_y_pos_real <=
              (MAX_REAL_Y_VALID*(1+POS_ERROR_TOLERANCE))):
        error_strings.append(
          "{0!s}Bad est_y_pos_real {1:n} from self_y_pos_real {2:n} (self_y_pos {3:n}), angle {4:n} ({5:n} {6:n}), dist {7:n}".format(
            start_string, est_y_pos_real, self_y_pos_real, self_y_pos, abs_angle, sin_angle, cos_angle, dist))
      if dist < 10:
        raise RuntimeError("\n".join(error_strings))
      else:
        if warn or (dist < 50):
          print("\n".join(error_strings), file=sys.stderr)
          sys.stderr.flush()
        return (None, None)

  poss_angles = list(poss_xy_pos_real.keys())
  if len(poss_angles) > 1:
    poss_angles.sort(key=lambda angle: abs(abs_angle-angle))
    poss_angles.sort(key=lambda angle: total_deviation_xy_pos_real[angle])
    poss_angles.sort(key=lambda angle: max_deviation_xy_pos_real[angle])
    poss_angles.sort(key=lambda angle: dist_xy_pos_real[angle])
  est_x_pos_real, est_y_pos_real = poss_xy_pos_real[poss_angles[0]]
  if warn:
    est_x_pos = get_x_normalized(est_x_pos_real)
    est_y_pos = get_y_normalized(est_y_pos_real)
  else:
    est_x_pos = get_x_normalized(est_x_pos_real, silent=True)
    est_y_pos = get_y_normalized(est_y_pos_real, silent=True)


  if warn:
    est_dist = get_dist_real(self_x_pos, self_y_pos,
                             est_x_pos, est_y_pos)

    if abs(dist - est_dist) > ERROR_TOLERANCE:
      est2_dist = get_dist_from_real(self_x_pos_real, self_y_pos_real,
                                     est_x_pos_real, est_y_pos_real)
      print(
        "{0!s}Difference in input ({1:n}), output ({2:n}) distances (est2 {3:n}) for get_abs_x_y_pos".format(
          start_string, dist, est_dist, est2_dist),
        file=sys.stderr)
      print(
        "Input angle {0:n}, used angle {1:n}, self_x_pos {2:n}, self_y_pos {3:n}".format(
          abs_angle, poss_angles[0], self_x_pos, self_y_pos),
        file=sys.stderr)
      print(
        "Self_x_pos_real {0:n}, self_y_pos_real {1:n}, est_x_pos_real {2:n}, est_y_pos_real {3:n}".format(
          self_x_pos_real, self_y_pos_real, est_x_pos_real, est_y_pos_real), file=sys.stderr)
      print("Est_x_pos {0:n}, est_y_pos {1:n}".format(est_x_pos, est_y_pos), file=sys.stderr)
      sys.stderr.flush()

  return (est_x_pos, est_y_pos)

landmark_start_to_location = {13: (GOAL_POS_X, get_y_normalized(0.0)), # center of goal
                              34: (get_x_normalized(0.0), get_y_normalized(-0.5*HALF_FIELD_WIDTH)), # Top Left
                              37: (get_x_normalized(HALF_FIELD_LENGTH), get_y_normalized(-0.5*HALF_FIELD_WIDTH)), # Top Right
                              40: (get_x_normalized(HALF_FIELD_LENGTH), get_y_normalized(HALF_FIELD_WIDTH/2)), # Bottom Right
                              43: (get_x_normalized(0,0), get_y_normalized(HALF_FIELD_WIDTH/2))} # Bottom Left


def filter_low_level_state(state, namespace):
  bit_list = []
  for pos in (0, 1, 9, 11, 12, 50, 54, 10): # TODO: Replace with constants!
    if state[pos] > 0:
      bit_list.append(True)
    else:
      bit_list.append(False)
##  if (state[0] > 0) and (max(abs(state[58]),abs(state[59]))
##                         >= sys.float_info.epsilon): # player2 angle valid
##    bit_list.append(True)
##  else:
##    bit_list.append(False)
  if len(bit_list) != BIT_LIST_LEN:
    raise RuntimeError(
      "Length of bit_list {0:n} not same as BIT_LIST_LEN {1:n}".format(
        len(bit_list), BIT_LIST_LEN))

  self_dict = {}
  if state[0] > 0: # self position valid

    if max(abs(state[5]),abs(state[6])) <= sys.float_info.epsilon:
      warnings.warn("Validity {0:d}{1:d} but invalid body angle {2:n}, {3:n}".format(
        int(bit_list[0]),int(bit_list[1]),state[5],state[6]))
      self_dict['body_angle'] = None
    else:
      self_dict['body_angle'] = get_angle(state[5],state[6])

    x_pos_from = {}
    x_pos_weight = {}
    y_pos_from = {}
    y_pos_weight = {}

    x_pos1_real = get_dist_from_proximity(state[46],HALF_FIELD_LENGTH)
    x_pos2_real = HALF_FIELD_LENGTH - get_dist_from_proximity(state[47],HALF_FIELD_LENGTH)
    x_pos_from['OOB'] = get_x_normalized((x_pos1_real+x_pos2_real)/2)
    x_pos_weight['OOB'] = max(ERROR_TOLERANCE,(1.0-min(1.0,abs(x_pos_from['OOB']))))

    y_pos1_real = get_dist_from_proximity(state[48],HALF_FIELD_WIDTH) - (HALF_FIELD_WIDTH/2)
    y_pos2_real = (HALF_FIELD_WIDTH/2) - get_dist_from_proximity(state[49],HALF_FIELD_WIDTH)
    y_pos_from['OOB'] = get_y_normalized((y_pos1_real+y_pos2_real)/2)
    y_pos_weight['OOB'] = max(ERROR_TOLERANCE,(1.0-min(1.0,abs(y_pos_from['OOB']))))

    x_pos_from['OTHER'] = namespace.other_dict['self_x_pos'].value
    y_pos_from['OTHER'] = namespace.other_dict['self_y_pos'].value
    other_proximity = get_proximity_from_dist(get_dist_real(x_pos_from['OTHER'],
                                                            y_pos_from['OTHER'],
                                                            namespace.other_dict['x_pos'].value,
                                                            namespace.other_dict['y_pos'].value))
    x_pos_weight['OTHER'] = y_pos_weight['OTHER'] = (other_proximity+1)/2
    if abs(x_pos_from['OTHER']) > (1.0-ERROR_TOLERANCE):
      x_pos_weight['OTHER'] = min(ERROR_TOLERANCE,x_pos_weight['OTHER'])
    if abs(y_pos_from['OTHER']) > (1.0-ERROR_TOLERANCE):
      y_pos_weight['OTHER'] = min(ERROR_TOLERANCE,y_pos_weight['OTHER'])

    if self_dict['body_angle'] is not None:
      for landmark_start, xy_location in iteritems(landmark_start_to_location):
        rel_angle = get_angle(state[landmark_start],state[landmark_start+1])
        abs_angle = get_abs_angle(self_dict['body_angle'],rel_angle)
        if abs_angle is not None:
          rev_angle = reverse_angle(abs_angle)
          dist = get_dist_from_proximity(state[landmark_start+2])
          x_pos, y_pos = get_abs_x_y_pos(abs_angle=rev_angle,
                                         dist=dist,
                                         self_x_pos=xy_location[0],
                                         self_y_pos=xy_location[1],
                                         warn=False,
                                         of_what="Rev " + str(landmark_start))
          if x_pos is not None:
            x_pos_from[landmark_start] = x_pos
            y_pos_from[landmark_start] = y_pos
            x_pos_weight[landmark_start] = y_pos_weight[landmark_start] = max(ERROR_TOLERANCE,
                                                                              ((state[landmark_start+2]+1)/2))
            # except extremely close can be a problem for the angle...
            if state[landmark_start+2] > (1.0-POS_ERROR_TOLERANCE):
              max_weight = (1.0-state[landmark_start+2])/POS_ERROR_TOLERANCE
              max_weight = max(max_weight, ERROR_TOLERANCE)
              x_pos_weight[landmark_start] = y_pos_weight[landmark_start] = min(x_pos_weight[landmark_start],
                                                                                max_weight)

    x_pos_total = 0.0
    x_pos_weight_total = 0.0
    for from_which, pos in iteritems(x_pos_from):
      x_pos_total += pos*x_pos_weight[from_which]
      x_pos_weight_total += x_pos_weight[from_which]
    self_dict['x_pos'] = x_pos_total/x_pos_weight_total
    for from_which, pos in iteritems(x_pos_from):
      if ((x_pos_weight[from_which]/x_pos_weight_total)*abs(pos-self_dict['x_pos'])) > POS_ERROR_TOLERANCE:
        print("Source {0!r} (weight {1:n}) x_pos {2:n} vs overall {3:n}".format(from_which,
                                                                                x_pos_weight[from_which]/
                                                                                x_pos_weight_total,
                                                                                pos,
                                                                                self_dict['x_pos']),
              file=sys.stderr)
        sys.stderr.flush()
    y_pos_total = 0.0
    y_pos_weight_total = 0.0
    for from_which, pos in iteritems(y_pos_from):
      y_pos_total += pos*y_pos_weight[from_which]
      y_pos_weight_total += y_pos_weight[from_which]
    self_dict['y_pos'] = y_pos_total/y_pos_weight_total
    for from_which, pos in iteritems(y_pos_from):
      if ((y_pos_weight[from_which]/y_pos_weight_total)*abs(pos-self_dict['y_pos'])) > POS_ERROR_TOLERANCE:
        print("Source {0!r} (weight {1:n}) y_pos {2:n} vs overall {3:n}".format(from_which,
                                                                                y_pos_weight[from_which]/
                                                                                y_pos_weight_total,
                                                                                pos,
                                                                                self_dict['y_pos']),
              file=sys.stderr)
        sys.stderr.flush()

    if max(abs(state[58]),abs(state[59])) <= sys.float_info.epsilon:
      warnings.warn("Validity {0:d}{1:d} but invalid other angle {2:n}, {3:n}".format(
        int(bit_list[0]),int(bit_list[1]),state[58],state[59]))
      self_dict['other_angle'] = None
    else:
      self_dict['other_angle'] = get_angle(state[58],state[59])
  else:
    self_dict['x_pos'] = None
    self_dict['y_pos'] = None
    if max(abs(state[5]),abs(state[6])) > sys.float_info.epsilon:
      warnings.warn("Validity {0:d}{1:d} but valid body angle {2:n}, {3:n}".format(
        int(bit_list[0]),int(bit_list[1]),state[5],state[6]))
      self_dict['body_angle'] = get_angle(state[5],state[6])
    else:
      self_dict['body_angle'] = None
    if max(abs(state[58]),abs(state[59])) > sys.float_info.epsilon:
      warnings.warn("Validity {0:d}{1:d} but valid other angle {2:n}, {3:n}".format(
        int(bit_list[0]),int(bit_list[1]),state[58],state[59]))
      self_dict['other_angle'] = get_angle(state[58],state[59])
    else:
      self_dict['other_angle'] = None

  if state[1] > 0: # self velocity valid
    self_dict['vel_rel_angle'] = get_angle(state[2],state[3])
    self_dict['vel_mag'] = state[4]
  else:
    self_dict['vel_rel_angle'] = None
    self_dict['vel_mag'] = -1

  goal_dict = {}
  if (state[0] > 0) and (max(state[18],state[21]) > -1): # self position valid, goal pos possible
    if state[18] > state[21]: # top post closer
      which_goal = "Top"
      goal_dict['dist'] = get_dist_from_proximity(state[18])
      goal_dict['rel_angle'] = get_angle(state[16],state[17])
    else:
      which_goal = "Bottom"
      goal_dict['dist'] = get_dist_from_proximity(state[21])
      goal_dict['rel_angle'] = get_angle(state[19],state[20])
    if (state[11] < 0) and (goal_dict['rel_angle']
                            is not None) and (goal_dict['dist']
                                              < (0.15+0.03)) and (get_angle_diff(0.0,
                                                                                goal_dict['rel_angle'])
                                                                 <= 1.0):
      raise RuntimeError("Should be in collision with goal - distance is {0:n}".format(goal_dict['dist']))
    elif goal_dict['dist'] > (MAX_RADIUS-POS_ERROR_TOLERANCE):
      raise RuntimeError(
        "Should not be possible to have dist {0:n} for goal (state[18] {1:n}, state[21] {2:n})".format(
          goal_dict['dist'], state[18], state[21]))
    goal_dict['abs_angle'] = get_abs_angle(self_dict['body_angle'],goal_dict['rel_angle'])
    if goal_dict['abs_angle'] is not None:

      est_x_pos, est_y_pos = get_abs_x_y_pos(abs_angle=goal_dict['abs_angle'],
                                             dist=goal_dict['dist'],
                                             self_x_pos=self_dict['x_pos'],
                                             self_y_pos=self_dict['y_pos'],
                                             warn=bool(goal_dict['dist'] < 10),
                                             of_what=which_goal+" goal post")

      if est_x_pos is not None:
        if (abs(est_x_pos - GOAL_POS_X) > POS_ERROR_TOLERANCE) and (goal_dict['dist'] < 10):
          print(
            "Estimated x_pos of goal is {0:n} (should be {1:n}; from est abs_angle {2:n}, dist {3:n}, self_x_pos {4:n})".format(
              est_x_pos,GOAL_POS_X,goal_dict['abs_angle'],goal_dict['dist'],self_dict['x_pos']),
            file=sys.stderr)
          sys.stderr.flush()
      goal_dict['x_pos'] = GOAL_POS_X
      if (state[18] > state[21]):
        goal_dict['y_pos'] = GOAL_TOP_POS_Y
        if est_y_pos is not None:
          if (abs(est_y_pos-GOAL_TOP_POS_Y) > POS_ERROR_TOLERANCE) and (goal_dict['dist'] < 10):
            print(
              "Estimated y_pos of top goalpost is {0:n} (should be {1:n}; from est abs_angle {2:n}, dist {3:n}, self_y_pos {4:n})".format(
                est_y_pos,GOAL_TOP_POS_Y,goal_dict['abs_angle'],goal_dict['dist'],self_dict['y_pos']),
              file=sys.stderr)
            sys.stderr.flush()
      else:
        goal_dict['y_pos'] = GOAL_BOTTOM_POS_Y
        if est_y_pos is not None:
          if (abs(est_y_pos-GOAL_BOTTOM_POS_Y) > POS_ERROR_TOLERANCE) and (goal_dict['dist'] < 10):
            print(
              "Estimated y_pos of bottom goalpost is {0:n} (should be {1:n}; from est abs_angle {2:n}, dist {3:n}, self_y_pos {4:n})".format(
                est_y_pos,GOAL_BOTTOM_POS_Y,goal_dict['abs_angle'],goal_dict['dist'],self_dict['y_pos']),
              file=sys.stderr)
            sys.stderr.flush()
      if (state[11] < 0) and (get_angle_diff(0.0, goal_dict['rel_angle'])
                              <= 1.0) and (get_dist_real(self_dict['x_pos'],
                                                         self_dict['y_pos'],
                                                         goal_dict['x_pos'],
                                                         goal_dict['y_pos']) < (0.15+0.03)):
        print(
          "Should be in collision with goal ({0:n}, {1:n} vs {2:n}, {3:n}; angle {4:n})".format(
            self_dict['x_pos'],self_dict['y_pos'],
            goal_dict['x_pos'],goal_dict['y_pos'],
            goal_dict['rel_angle']),
          file=sys.stderr)
        sys.stderr.flush()
    else:
      goal_dict['x_pos'] = GOAL_POS_X
      if state[18] > state[21]:
        goal_dict['y_pos'] = GOAL_TOP_POS_Y
      else:
        goal_dict['y_pos'] = GOAL_BOTTOM_POS_Y

  else:
    goal_dict['dist'] = get_dist_from_proximity(-1)
    goal_dict['rel_angle'] = None
    goal_dict['abs_angle'] = None
    goal_dict['x_pos'] = GOAL_POS_X
    if random.random() < 0.5:
      goal_dict['y_pos'] = GOAL_BOTTOM_POS_Y
    else:
      goal_dict['y_pos'] = GOAL_TOP_POS_Y

  ball_dict = {}
  if state[50] > 0: # ball position valid
    ball_dict['dist'] = get_dist_from_proximity(state[53])
    ball_dict['rel_angle'] = get_angle(state[51],state[52])
    if (state[9] < 0) and (ball_dict['rel_angle']
                           is not None) and (ball_dict['dist']
                                             < (0.15+0.0425)) and (get_angle_diff(0.0, ball_dict['rel_angle'])
                                                                 <= 1.0):
      raise RuntimeError("Should be in collision with ball - distance is {0:n}".format(ball_dict['dist']))
    ball_dict['abs_angle'] = get_abs_angle(self_dict['body_angle'],ball_dict['rel_angle'])
    if ball_dict['abs_angle'] is not None:
      ball_dict['x_pos'], ball_dict['y_pos'] = get_abs_x_y_pos(abs_angle=ball_dict['abs_angle'],
                                                               dist=ball_dict['dist'],
                                                               self_x_pos=self_dict['x_pos'],
                                                               self_y_pos=self_dict['y_pos'],
                                                               warn=bool(ball_dict['dist'] < 10),
                                                               of_what='Ball')
      if (ball_dict['x_pos'] is None) or (ball_dict['y_pos'] is None):
        if (ball_dict['dist'] < 10):
          raise RuntimeError("Unknown ball position despite state[50] > 0")
      else:
        if (state[9] < 0) and (get_angle_diff(0.0, ball_dict['rel_angle'])
                               <= 1.0) and (get_dist_real(self_dict['x_pos'],self_dict['y_pos'],
                                                          ball_dict['x_pos'],ball_dict['y_pos']) < (0.15+0.0425)):
          raise RuntimeError(
            "Should be in collision with ball ({0:n}, {1:n} vs {2:n}, {3:n}; rel_angle {4:n})".format(
              self_dict['x_pos'],self_dict['y_pos'],
              ball_dict['x_pos'],ball_dict['y_pos'],
              ball_dict['rel_angle']))
  else:
    ball_dict['dist'] = get_dist_from_proximity(-1)
    ball_dict['rel_angle'] = None
    ball_dict['abs_angle'] = None
    ball_dict['x_pos'] = None
    ball_dict['y_pos'] = None

  if (ball_dict['x_pos'] is None) and (abs(namespace.other_dict['ball_x_pos'].value) < 1):
    ball_dict['x_pos'] = namespace.other_dict['ball_x_pos'].value
  if (ball_dict['y_pos'] is None) and (abs(namespace.other_dict['ball_y_pos'].value) < 1):
    ball_dict['y_pos'] = namespace.other_dict['ball_y_pos'].value

  if state[54] > 0: # ball velocity valid
    ball_dict['vel_rel_angle'] = get_angle(state[55],state[56])
    ball_dict['vel_mag'] = state[57]
  else:
    ball_dict['vel_rel_angle'] = None
    ball_dict['vel_mag'] = 0

  return (bit_list, self_dict, goal_dict, ball_dict)


def bool_list_to_int(bit_list):
  return sum(2**i*b for i, b in enumerate(bit_list))

def int_to_bool_list(intval, num_bits=BIT_LIST_LEN):
  if num_bits < intval.bit_length():
    raise RuntimeError(
      "Integer value {0:d} will not fit into {1:d} bits (needs {2:d})".format(intval,
                                                                              num_bits,
                                                                              intval.bit_length()))
  result = []
  for bit in range(num_bits):
    mask = 1 << bit
    result.append(bool((intval & mask) == mask))
  return result

def pack_action_bit_list(action, bit_list):
  bool_int = bool_list_to_int(bit_list)

  return struct.pack(STRUCT_PACK, action, bool_int)

def unpack_action_bit_list(bytestring):
  action, bool_int = struct.unpack(STRUCT_PACK, bytestring)

  bit_list = int_to_bool_list(bool_int)

  return (action, bit_list)


def evaluate_previous_action(hfo_env,
                             state,
                             namespace):
  if namespace.action in (hfo.NOOP, hfo.QUIT):
    warnings.warn("evaluate_previous_action should not have been called with 'action' NOOP/QUIT")
    return
  bit_list, self_dict, goal_dict, ball_dict = filter_low_level_state(state, namespace)
  action_status = hfo_env.getLastActionStatus(namespace.action)

  action_string = hfo_env.actionToString(namespace.action)
  if namespace.action_params:
    action_string += '(' + ",".join(list(["{:n}".format(x) for x in namespace.action_params])) + ')'

  action_status_observed = hfo.ACTION_STATUS_UNKNOWN # NOTE: Check intent + other bitflags!


  if (namespace.action in
      (hfo.DRIBBLE, hfo.DRIBBLE_TO)) and namespace.prestate_bit_list[4] and (not bit_list[4]):
    action_status_observed = hfo.ACTION_STATUS_BAD
  elif (namespace.action in
        (hfo.DRIBBLE,
         hfo.INTERCEPT,
         hfo.MOVE)) and (((namespace.intent != INTENT_GOAL_COLLISION)
                          and (not namespace.prestate_bit_list[3])
                          and bit_list[3]) or
                         ((namespace.intent != INTENT_BALL_COLLISION)
                          and (not namespace.prestate_bit_list[2])
                          and bit_list[2]) or
                         ((namespace.intent != INTENT_PLAYER_COLLISION)
                          and (not namespace.prestate_bit_list[7])
                          and bit_list[7])):
    action_status_observed = hfo.ACTION_STATUS_BAD
  elif (namespace.intent !=
      INTENT_GOAL_COLLISION) and namespace.prestate_bit_list[3] and (not bit_list[3]):
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.intent !=
        INTENT_BALL_COLLISION) and namespace.prestate_bit_list[2] and (not bit_list[2]):
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.intent !=
        INTENT_PLAYER_COLLISION) and namespace.prestate_bit_list[7] and (not bit_list[7]):
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.intent == INTENT_BALL_KICKABLE) and (not namespace.prestate_bit_list[4]) and bit_list[4]:
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.intent == INTENT_GOAL_COLLISION) and (not namespace.prestate_bit_list[3]) and bit_list[3]:
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.intent == INTENT_BALL_COLLISION) and (((not namespace.prestate_bit_list[2]) and bit_list[2]) or
                                                        ((not namespace.prestate_bit_list[4]) and bit_list[4])):
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.intent == INTENT_PLAYER_COLLISION) and (not namespace.prestate_bit_list[7]) and bit_list[7]:
    action_status_observed = hfo.ACTION_STATUS_MAYBE
  elif (namespace.action == hfo.DASH) or (namespace.action == hfo.MOVE):
      pass # TODO
  elif namespace.action == hfo.TURN:
    if (namespace.prestate_self_dict['body_angle'] is not None) and (self_dict['body_angle'] is not None):
      intended_body_angle = namespace.prestate_self_dict['body_angle'] + namespace.action_params[0]
      if get_angle_diff(namespace.prestate_self_dict['body_angle'],
                        intended_body_angle) > get_angle_diff(self_dict['body_angle'],
                                                              intended_body_angle):
        action_status_observed = hfo.ACTION_STATUS_MAYBE
      else:
        action_status_observed = hfo.ACTION_STATUS_BAD
  elif namespace.action == hfo.PASS:
    if (namespace.prestate_ball_dict['x_pos'] is not None) and (ball_dict['x_pos'] is not None):
      dist_after = get_dist_real(ball_dict['x_pos'],
                                 ball_dict['y_pos'],
                                 namespace.other_dict['x_pos'].value,
                                 namespace.other_dict['y_pos'].value)
      self_dist_after = get_dist_real(ball_dict['x_pos'],
                                      ball_dict['y_pos'],
                                      self_dict['x_pos'],
                                      self_dict['y_pos'])
      if (dist_after < self_dist_after):
        action_status_observed = hfo.ACTION_STATUS_MAYBE
      else:
        action_status_observed = hfo.ACTION_STATUS_BAD
    elif bit_list[4]:
      action_status_observed = hfo.ACTION_STATUS_BAD
  elif namespace.action == hfo.KICK:
    if bit_list[4]:
      action_status_observed = hfo.ACTION_STATUS_BAD
    else:
      pass # TODO
  elif namespace.action == hfo.KICK_TO:
    if (namespace.prestate_ball_dict['x_pos'] is not None) and (ball_dict['x_pos'] is not None):
      dist_before = get_dist_real(namespace.prestate_ball_dict['x_pos'],
                                  namespace.prestate_ball_dict['y_pos'],
                                  namespace.action_params[0],
                                  namespace.action_params[1])
      dist_after = get_dist_real(ball_dict['x_pos'],
                                 ball_dict['y_pos'],
                                 namespace.action_params[0],
                                 namespace.action_params[1])
      if dist_before > dist_after:
        action_status_observed = hfo.ACTION_STATUS_MAYBE
      else:
        action_status_observed = hfo.ACTION_STATUS_BAD
  elif namespace.action == hfo.MOVE_TO:
    if (namespace.prestate_self_dict['x_pos'] is not None) and (self_dict['x_pos'] is not None):
      dist_before = get_dist_real(namespace.prestate_self_dict['x_pos'],
                                  namespace.prestate_self_dict['y_pos'],
                                  namespace.action_params[0],
                                  namespace.action_params[1])
      dist_after = get_dist_real(self_dict['x_pos'],
                                 self_dict['y_pos'],
                                 namespace.action_params[0],
                                 namespace.action_params[1])
      if dist_before > dist_after:
        action_status_observed = hfo.ACTION_STATUS_MAYBE
      else:
        action_status_observed = hfo.ACTION_STATUS_BAD
    else:
      pass # ???
  elif namespace.action == hfo.DRIBBLE_TO:
    if namespace.prestate_bit_list[4]:
      if not bit_list[4]:
        action_status_observed = hfo.ACTION_STATUS_BAD # should have been done above, actually
      elif (namespace.prestate_self_dict['x_pos'] is not None) and (self_dict['x_pos'] is not None):
        dist_before = get_dist_real(namespace.prestate_self_dict['x_pos'],
                                    namespace.prestate_self_dict['y_pos'],
                                    namespace.action_params[0],
                                    namespace.action_params[1])
        dist_after = get_dist_real(self_dict['x_pos'],
                                   self_dict['y_pos'],
                                   namespace.action_params[0],
                                   namespace.action_params[1])
        if dist_before > dist_after:
          action_status_observed = hfo.ACTION_STATUS_MAYBE
        else:
          action_status_observed = hfo.ACTION_STATUS_BAD
      elif (namespace.prestate_ball_dict['x_pos'] is not None) and (ball_dict['x_pos'] is not None):
        dist_before = get_dist_real(namespace.prestate_ball_dict['x_pos'],
                                    namespace.prestate_ball_dict['y_pos'],
                                    namespace.action_params[0],
                                    namespace.action_params[1])
        dist_after = get_dist_real(ball_dict['x_pos'],
                                   ball_dict['y_pos'],
                                   namespace.action_params[0],
                                   namespace.action_params[1])
        if dist_before > dist_after:
          action_status_observed = hfo.ACTION_STATUS_MAYBE
        else:
          action_status_observed = hfo.ACTION_STATUS_BAD
      else:
        pass # ???
    elif bit_list[4]:
      action_status_observed = hfo.ACTION_STATUS_MAYBE
    elif namespace.prestate_ball_dict['dist'] > ball_dict['dist']:
      action_status_observed = hfo.ACTION_STATUS_MAYBE
    elif namespace.prestate_ball_dict['dist'] < ball_dict['dist']:
      action_status_observed = hfo.ACTION_STATUS_BAD
    elif ball_dict['dist'] < ((1-ERROR_TOLERANCE)*MAX_RADIUS):
      action_status_observed = hfo.ACTION_STATUS_BAD
  elif namespace.action in (hfo.INTERCEPT, hfo.GO_TO_BALL):
    if namespace.prestate_bit_list[4]:
      if bit_list[4]:
        if (namespace.intent != INTENT_BALL_COLLISION) and bit_list[2]:
          action_status_observed = hfo.ACTION_STATUS_BAD
        elif (namespace.intent != INTENT_GOAL_COLLISION) and bit_list[3]:
          action_status_observed = hfo.ACTION_STATUS_BAD
        else:
          action_status_observed = hfo.ACTION_STATUS_MAYBE
      else:
        action_status_observed = hfo.ACTION_STATUS_BAD
    elif bit_list[4]:
      action_status_observed = hfo.ACTION_STATUS_MAYBE
    elif namespace.prestate_ball_dict['dist'] > ball_dict['dist']:
      action_status_observed = hfo.ACTION_STATUS_MAYBE
    elif namespace.prestate_ball_dict['dist'] < ball_dict['dist']:
      action_status_observed = hfo.ACTION_STATUS_BAD
    elif ball_dict['dist'] < ((1-ERROR_TOLERANCE)*MAX_RADIUS):
      action_status_observed = hfo.ACTION_STATUS_BAD
  elif namespace.action == hfo.DRIBBLE:
    pass # todo?
  else:
    raise RuntimeError("Unknown how to evaluate {0!s}".format(action_string))

  if action_status_observed == hfo.ACTION_STATUS_UNKNOWN:
    action_status_guessed = action_status
  else:
    action_status_guessed = action_status_observed
    if (action_status != action_status_observed) and (action_status != hfo.ACTION_STATUS_UNKNOWN):
      print(
        "{0!s}: Difference between feedback ({1!s}), observed ({2!s}) action_status (prestate bit_list {3!s}, current bit list {4!s})".format(
          action_string,
          hfo_env.actionStatusToString(action_status),
          hfo_env.actionStatusToString(action_status_observed),
          "".join(map(str,map(int,namespace.prestate_bit_list))),
          "".join(map(str,map(int,bit_list)))),
        file=sys.stderr)
      sys.stderr.flush()

  if (action_status_guessed == hfo.ACTION_STATUS_BAD) and (not namespace.checking_intent):
    # unexpected lack of success
    print(
      "Unexpected lack of success for last action {0!s} (prestate bit_list {1!s}, current bit list {2!s})".format(
        action_string,
        "".join(map(str,map(int,namespace.prestate_bit_list))),
        "".join(map(str,map(int,bit_list)))),
      file=sys.stderr)
    if namespace.intent == INTENT_GOAL_COLLISION:
      print("Prestate goal distance is {0:n}, current goal distance is {1:n}".format(namespace.prestate_goal_dict['dist'],
                                                                                     goal_dict['dist']),
            file=sys.stderr)
    elif namespace.intent == INTENT_BALL_COLLISION:
      print("Prestate ball distance is {0:n}, current ball distance is {1:n}".format(namespace.prestate_ball_dict['dist'],
                                                                                     ball_dict['dist']),
            file=sys.stderr)
    sys.stderr.flush()
    namespace.action_tried[namespace.action] += 1
    namespace.struct_tried[pack_action_bit_list(namespace.action,namespace.prestate_bit_list)] += 1
    # further analysis...

  if action_status_guessed == hfo.ACTION_STATUS_MAYBE:
    # further analysis...
    if namespace.prestate_bit_list[3] and (namespace.action in (hfo.KICK_TO, hfo.KICK)):
      print("OK status from {0!s} despite goal collision in prestate (poststate: {1!s})".format(
        action_string, bit_list[3]))
      sys.stdout.flush()

    if namespace.prestate_bit_list[2] and (namespace.action == hfo.MOVE):
      print("OK status from {0!s} despite ball collision in prestate (poststate: {1!s})".format(
        action_string, bit_list[2]))
      sys.stdout.flush()

    if namespace.prestate_bit_list[7]:
      print("OK status from {0!s} despite player collision in prestate (poststate: {1!s})".format(
        action_string, bit_list[2]))
      sys.stdout.flush()
  elif action_status_guessed == hfo.ACTION_STATUS_BAD:
    if namespace.action in (hfo.MOVE_TO, hfo.INTERCEPT, hfo.MOVE, hfo.DRIBBLE, hfo.PASS, hfo.KICK_TO, hfo.KICK):
      print("Bad status from {0!s}: prestate {1!s}, poststate {2!s}".format(
        action_string,
        "".join(map(str,map(int,namespace.prestate_bit_list))),
        "".join(map(str,map(int,bit_list)))))
      sys.stdout.flush()

def save_action_prestate(action,
                         prestate_bit_list,
                         prestate_self_dict,
                         prestate_goal_dict,
                         prestate_ball_dict,
                         intent,
                         checking_intent,
                         namespace,
                         action_params=None):
  namespace.action = action
  namespace.prestate_bit_list = prestate_bit_list
  namespace.prestate_self_dict = prestate_self_dict
  namespace.prestate_goal_dict = prestate_goal_dict
  namespace.prestate_ball_dict = prestate_ball_dict
  if action_params is None:
    action_params = []
  namespace.action_params = action_params
  namespace.intent = intent
  if intent is not None:
    namespace.intent_done[intent] += 1
  namespace.checking_intent = checking_intent
  if checking_intent:
    namespace.action_tried[action] += 1
    namespace.struct_tried[pack_action_bit_list(action, prestate_bit_list)] += 1
  if action_params:
    return [action] + action_params
  return action

def determine_which_action(poss_actions_list, namespace, bit_list):
  """
  Decides between poss_actions based on lowest struct_tried,
  then lowest action_tried, then random.
  """
  if len(poss_actions_list) > 1:
    random.shuffle(poss_actions_list)
    poss_actions_list.sort(key=lambda action: namespace.struct_tried[pack_action_bit_list(action, bit_list)])
   
    if (namespace.struct_tried[pack_action_bit_list(poss_actions_list[0],
                                                    bit_list)]
        == namespace.struct_tried[pack_action_bit_list(poss_actions_list[1], bit_list)]):
      poss_actions_list.sort(key=lambda action: namespace.action_tried[action])

  return poss_actions_list[0]

def do_intent(hfo_env,
              state,
              namespace):
  bit_list, self_dict, goal_dict, ball_dict = filter_low_level_state(state, namespace)
  prestate_dict = {'prestate_bit_list': bit_list,
                   'prestate_self_dict': self_dict,
                   'prestate_goal_dict': goal_dict,
                   'prestate_ball_dict': ball_dict,
                   'checking_intent': False,
                   'intent': namespace.intent,
                   'namespace': namespace}

  if (namespace.intent == INTENT_BALL_COLLISION) and bit_list[4]: # ball is kickable
    ball_rel_angle = ball_dict['rel_angle']
    if ball_rel_angle > 180:
      ball_rel_angle -= 360
    poss_actions_list = []
    if bit_list[0] and (ball_dict['x_pos'] is not None): # self position valid
      if get_dist_real(self_dict['x_pos'],self_dict['y_pos'],
                       ball_dict['x_pos'],ball_dict['y_pos']) < (0.15+0.0425):
        if abs(ball_rel_angle) > 0.5:
          hfo_env.act(*save_action_prestate(action=hfo.TURN,action_params=[ball_rel_angle],
                                            **prestate_dict))
        else:
          print(
            "Should already be in collision with ball ({0:n}, {1:n} vs {2:n}, {3:n})".format(
              self_dict['x_pos'],self_dict['y_pos'],
              ball_dict['x_pos'],ball_dict['y_pos']),
            file=sys.stderr)
          sys.stderr.flush()
          hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                            action_params=[80, ball_rel_angle],
                                            **prestate_dict))
        return

      poss_actions_list.append(hfo.MOVE_TO)

    if abs(ball_rel_angle) > 0.5:
      poss_actions_list.append(hfo.TURN)
    if abs(ball_rel_angle) < 1.0:
      poss_actions_list.append(hfo.DASH)

    action = determine_which_action(poss_actions_list, namespace, bit_list)

    if action == hfo.MOVE_TO:
      hfo_env.act(*save_action_prestate(action=hfo.MOVE_TO,
                                        action_params=[ball_dict['x_pos'],
                                                       ball_dict['y_pos']],
                                        **prestate_dict))
    elif action == hfo.TURN:
      hfo_env.act(*save_action_prestate(action=hfo.TURN,
                                        action_params=[ball_rel_angle],
                                        **prestate_dict))
    elif action == hfo.DASH:
      hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                        action_params=[80, ball_rel_angle],
                                        **prestate_dict))
    else:
      raise RuntimeError("Unknown action {0!r}".format(action))

    return


  elif (namespace.intent == INTENT_BALL_KICKABLE) or (namespace.intent == INTENT_BALL_COLLISION):
    poss_actions_list = [hfo.INTERCEPT]
    if bit_list[0]:
      poss_actions_list.append(hfo.GO_TO_BALL)

    ball_rel_angle = ball_dict['rel_angle']
    if ball_rel_angle > 180:
      ball_rel_angle -= 360
    if abs(ball_rel_angle) > 0.5:
      poss_actions_list.append(hfo.TURN)
    if abs(ball_rel_angle) < 1.0:
      poss_actions_list.append(hfo.DASH)
    if bit_list[0] and (ball_dict['x_pos'] is not None):
      poss_actions_list.append(hfo.MOVE_TO)

    action = determine_which_action(poss_actions_list, namespace, bit_list)

    if (action == hfo.GO_TO_BALL) or (action == hfo.INTERCEPT):
      hfo_env.act(save_action_prestate(action=action,**prestate_dict))
    elif action == hfo.TURN:
      hfo_env.act(*save_action_prestate(action=hfo.TURN,
                                        action_params=[ball_rel_angle],
                                        **prestate_dict))
    elif action == hfo.DASH:
      hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                        action_params=[80, ball_rel_angle],
                                        **prestate_dict))
    elif action == hfo.MOVE_TO:
      hfo_env.act(*save_action_prestate(action=hfo.MOVE_TO,
                                        action_params=[ball_dict['x_pos'],
                                                       ball_dict['y_pos']],
                                        **prestate_dict))
    else:
      raise RuntimeError("Unknown action {0!r}".format(action))

    return

  elif namespace.intent == INTENT_GOAL_COLLISION:
    goal_rel_angle = goal_dict['rel_angle']
    if goal_rel_angle > 180:
      goal_rel_angle -= 360
    poss_actions_list = []

    if (get_dist_real(self_dict['x_pos'],
                      self_dict['y_pos'],
                      goal_dict['x_pos'],
                      goal_dict['y_pos']) < (0.15+0.03)):
      if abs(goal_rel_angle) > 0.5:
        hfo_env.act(*save_action_prestate(action=hfo.TURN,
                                          action_params=[goal_rel_angle],
                                          **prestate_dict))
      else:
        print(
          "Should already be in collision with goal ({0:n}, {1:n} vs {2:n}, {3:n})".format(
            self_dict['x_pos'],self_dict['y_pos'],
            goal_dict['x_pos'],goal_dict['y_pos']),
          file=sys.stderr)
        sys.stderr.flush()
        if not bit_list[4]:
          hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                            action_params=[80, goal_rel_angle],
                                            **prestate_dict))
        else:
          hfo_env.act(*save_action_prestate(action=hfo.DRIBBLE_TO,
                                            action_params=[min(1.0,max(-1.0,(self_dict['x_pos']+
                                                                             (2*(goal_dict['x_pos']-
                                                                                 self_dict['x_pos']))))),
                                                           min(1.0,max(-1.0,(self_dict['y_pos']+
                                                                             (2*(goal_dict['y_pos']-
                                                                                 self_dict['y_pos'])))))],
                                            **prestate_dict))

      return

    poss_actions_list.append(hfo.DRIBBLE_TO)
    if not bit_list[4]:
      poss_actions_list.append(hfo.MOVE_TO)

    if goal_rel_angle is not None:
      if abs(goal_rel_angle) > 0.5:
        poss_actions_list.append(hfo.TURN)
      if (abs(goal_rel_angle) < 1.0) and ((not bit_list[4]) or (goal_dict['dist'] < 0.36)):
        poss_actions_list.append(hfo.DASH)

    action = determine_which_action(poss_actions_list, namespace, bit_list)

    if (action == hfo.TURN):
      hfo_env.act(*save_action_prestate(action=hfo.TURN,
                                        action_params=[goal_rel_angle],
                                        **prestate_dict))
    elif action in (hfo.DRIBBLE_TO, hfo.MOVE_TO):
      x_pos_target = min(1.0,(goal_dict['x_pos']+POS_ERROR_TOLERANCE),
                         max(-1.0,(goal_dict['x_pos']-POS_ERROR_TOLERANCE),
                             (self_dict['x_pos']+(2*(goal_dict['x_pos']-self_dict['x_pos'])))))
      y_pos_target = min(1.0,(goal_dict['y_pos']+POS_ERROR_TOLERANCE),
                         max(-1.0,(goal_dict['y_pos']-POS_ERROR_TOLERANCE),
                             (self_dict['y_pos']+(2*(goal_dict['y_pos']-self_dict['y_pos'])))))
      hfo_env.act(*save_action_prestate(action=action,
                                        action_params=[x_pos_target,y_pos_target],
                                        **prestate_dict))
    elif (action == hfo.DASH):
      hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                        action_params=[80, goal_rel_angle],
                                        **prestate_dict))
    else:
      raise RuntimeError("Unknown action {0!r}".format(action))

    return

  elif namespace.intent == INTENT_PLAYER_COLLISION:
    rel_angle = self_dict['other_angle']
    if rel_angle > 180:
      rel_angle -= 360

    other_dist = get_dist_real(self_dict['x_pos'],
                               self_dict['y_pos'],
                               namespace.other_dict['x_pos'].value,
                               namespace.other_dict['y_pos'].value)

    if (other_dist < 0.3):
      if abs(rel_angle) > 0.5:
        hfo_env.act(*save_action_prestate(action=hfo.TURN,
                                          action_params=[rel_angle],
                                          **prestate_dict))
      else:
        print(
          "Should already be in collision with player2 ({0:n}, {1:n} vs {2:n}, {3:n})".format(
            self_dict['x_pos'],self_dict['y_pos'],
            namespace.other_dict['x_pos'].value,
            namespace.other_dict['y_pos'].value),
          file=sys.stderr)
        sys.stderr.flush()
        if not bit_list[4]:
          hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                            action_params=[80, rel_angle],
                                            **prestate_dict))
        else:
          hfo_env.act(*save_action_prestate(action=hfo.DRIBBLE_TO,
                                            action_params=[(self_dict['x_pos']+
                                                            (2*(namespace.other_dict['x_pos'].value-
                                                                self_dict['x_pos']))),
                                                           (self_dict['y_pos']+
                                                            (2*(namespace.other_dict['y_pos'].value-
                                                                self_dict['y_pos'])))],
                                            **prestate_dict))

      return
    
    poss_actions_list = []

    poss_actions_list.append(hfo.DRIBBLE_TO)
    if not bit_list[4]:
      poss_actions_list.append(hfo.MOVE_TO)
    if rel_angle is not None:
      if abs(rel_angle) > 0.5:
        poss_actions_list.append(hfo.TURN)
      if (abs(rel_angle) < 1.0) and ((not bit_list[4]) or (other_dist < 0.6)):
        poss_actions_list.append(hfo.DASH)
      elif bit_list[4] and (other_dist < 0.6):
        poss_actions_list.append(hfo.DASH)

    action = determine_which_action(poss_actions_list, namespace, bit_list)

    if action == hfo.TURN:
      hfo_env.act(*save_action_prestate(action=hfo.TURN,
                                        action_params=[rel_angle],
                                        **prestate_dict))
    elif (action == hfo.MOVE_TO) or (action == hfo.DRIBBLE_TO):
      hfo_env.act(*save_action_prestate(action=action,
                                        action_params=[namespace.other_dict['x_pos'].value,
                                                       namespace.other_dict['y_pos'].value],
                                        **prestate_dict))
    elif action == hfo.DASH:
      hfo_env.act(*save_action_prestate(action=hfo.DASH,
                                        action_params=[80, rel_angle],
                                        **prestate_dict))
    else:
      raise RuntimeError("Unknown action {0!r}".format(action))

    return

  else:
    raise RuntimeError("Unexpected intent {0!r}".format(namespace.intent))

def decide_intent(poss_intent_list, namespace):
    if len(poss_intent_list) > 1:
      random.shuffle(poss_intent_list)
      poss_intent_list.sort(key=lambda intent: namespace.intent_done[intent])
    return poss_intent_list[0]

def do_next_action(hfo_env,
                   state,
                   namespace):
  bit_list, self_dict, goal_dict, ball_dict = filter_low_level_state(state, namespace)

  if not bit_list[1]: # self velocity invalid
    # long-term: Turn, Kick, Kick_To
    raise NotImplementedError("Not yet set up for self velocity invalid")

  prior_intent = namespace.intent
  # admittedly, need to check for some of "intent" circumstances in combo!
  if namespace.intent == INTENT_BALL_COLLISION:
    if bit_list[2]:
      namespace.intent = None
    else:
      if (not bit_list[5]) or (ball_dict['x_pos'] is None): # first: ball location not valid
        namespace.intent_done[INTENT_BALL_COLLISION] -= 1
        if bit_list[0]: # self location valid
          if bit_list[3] or bit_list[7]:
            namespace.intent = None
          else:
            namespace.intent = decide_intent([INTENT_GOAL_COLLISION, INTENT_PLAYER_COLLISION],
                                             namespace)
        else:
          raise NotImplementedError("Not yet set up for self+ball location invalid")
  elif namespace.intent == INTENT_BALL_KICKABLE:
    if bit_list[4]:
      namespace.intent = None
    else:
      if (not bit_list[5]) or (ball_dict['x_pos'] is None): # first: ball location not valid
        namespace.intent_done[INTENT_BALL_KICKABLE] -= 1
        if bit_list[0]: # self location valid
          if bit_list[3] or bit_list[7]:
            namespace.intent = None
          else:
            namespace.intent = decide_intent([INTENT_GOAL_COLLISION, INTENT_PLAYER_COLLISION],
                                             namespace)
        else:
          raise NotImplementedError("Not yet set up for self+ball location invalid")
  elif namespace.intent == INTENT_GOAL_COLLISION:
    if bit_list[3]:
      namespace.intent = None
    else:
      if (not bit_list[0]) or (goal_dict['rel_angle'] is None): # first: self location not valid
        namespace.intent_done[INTENT_GOAL_COLLISION] -= 1
        if bit_list[4] or bit_list[2] or bit_list[7]:
          namespace.intent = None
        else:
          poss_intent_list = [INTENT_PLAYER_COLLISION]
          if bit_list[5] and (ball_dict['x_pos'] is not None):
            poss_intent_list += [INTENT_BALL_COLLISION, INTENT_BALL_KICKABLE]
          namespace.intent = decide_intent(poss_intent_list, namespace)
  elif namespace.intent == INTENT_PLAYER_COLLISION:
    if bit_list[7]:
      namespace.intent = None
    else:
      if not bit_list[0]: # self location not valid
        if bit_list[4] or bit_list[2] or bit_list[3]:
          namespace.intent = None
        else:
          poss_intent_list = [INTENT_GOAL_COLLISION]
          if bit_list[5] and (ball_dict['x_pos'] is not None):
            poss_intent_list += [INTENT_BALL_COLLISION, INTENT_BALL_KICKABLE]
          namespace.intent = decide_intent(poss_intent_list, namespace)
  if namespace.intent is not None:
    namespace.times_intent_NONE = 0
    return do_intent(hfo_env, state, namespace)

  # figure out what to do next

  actions_want_check = set([])
  if (bit_list[4] and bit_list[3]):
    actions_want_check |= set([hfo.KICK, hfo.KICK_TO])
  elif ((not bit_list[4]) and bit_list[2]):
    actions_want_check |= set([hfo.MOVE])
##  if bit_list[2] or bit_list[3]:
##    if bit_list[4]: # kickable
##      actions_want_check |= set([hfo.KICK, hfo.KICK_TO])
##    elif bit_list[2]:
##      actions_want_check |= set([hfo.MOVE])
##    if bit_list[7]: # collision w/player
##      if bit_list[4]:
##        actions_want_check |= set([hfo.DRIBBLE])
##      else:
##        actions_want_check |= set([hfo.GO_TO_BALL])

  if not actions_want_check:
    poss_intent_set = set([INTENT_BALL_KICKABLE,INTENT_BALL_COLLISION,
                           INTENT_GOAL_COLLISION])

    if (not bit_list[5]) or (ball_dict['x_pos'] is None):
      poss_intent_set.discard(INTENT_BALL_KICKABLE)
      poss_intent_set.discard(INTENT_BALL_COLLISION)
    elif bit_list[4]:
      poss_intent_set.discard(INTENT_BALL_KICKABLE)

    if not bit_list[0]:
      poss_intent_set.discard(INTENT_GOAL_COLLISION)
      poss_intent_set.discard(INTENT_PLAYER_COLLISION)
    elif self_dict['other_angle'] is None:
      poss_intent_set.discard(INTENT_PLAYER_COLLISION)

    if not poss_intent_set:
      raise NotImplementedError("Not yet set up for self+ball location invalid")

    namespace.intent = decide_intent(list(poss_intent_set), namespace)

    print("INTENT: {}".format(INTENT_DICT[namespace.intent]))
    sys.stdout.flush()
    namespace.times_intent_NONE = 0

    return do_intent(hfo_env, state, namespace)

  if bit_list[4] and (prior_intent not in (None, INTENT_BALL_KICKABLE)):
    namespace.intent_done[INTENT_BALL_KICKABLE] += 1
  if bit_list[2] and (prior_intent not in (None, INTENT_BALL_COLLISION)):
    namespace.intent_done[INTENT_BALL_COLLISION] += 1
  if bit_list[3] and (prior_intent not in (None, INTENT_GOAL_COLLISION)):
    namespace.intent_done[INTENT_GOAL_COLLISION] += 1
  if bit_list[7] and (prior_intent not in (None, INTENT_PLAYER_COLLISION)):
    namespace.intent_done[INTENT_PLAYER_COLLISION] += 1

  actions_maybe_check = set([hfo.TURN])
  if bit_list[4]:
    actions_maybe_check |= set([hfo.KICK])
  else:
    actions_maybe_check |= set([hfo.DASH])

  if bit_list[0]:
    actions_maybe_check |= set([hfo.MOVE_TO])
    if bit_list[5] and (ball_dict['x_pos'] is not None):
      actions_maybe_check |= set([hfo.DRIBBLE_TO])
      if bit_list[4]:
        actions_maybe_check |= set([hfo.KICK_TO,hfo.DRIBBLE])
        if (self_dict['other_angle'] is not None) and (1 <= namespace.other_dict['unum'].value <= 11):
          actions_maybe_check |= set([hfo.PASS])
      else:
        actions_maybe_check |= set([hfo.INTERCEPT,hfo.GO_TO_BALL,hfo.MOVE])

  if not bit_list[6]: # ball velocity invalid
    # long-term: Kick, Kick_To, Intercept - if know ball location; Kick_To requires self location valid
    raise NotImplementedError("Not yet set up for ball velocity invalid")

  poss_actions = actions_want_check & actions_maybe_check

  if not poss_actions:
    raise RuntimeError(
      "Unknown what to do - actions_want_check {0!r}, actions_maybe_check {1!r}".format(actions_want_check,
                                                                                        actions_maybe_check))

  action = determine_which_action(list(poss_actions), namespace, bit_list)

  if prior_intent is not None:
    namespace.times_intent_NONE = 0
    print("INTENT: WAS {}".format(INTENT_DICT[prior_intent]))
  else:
    namespace.times_intent_NONE += 1
    print("INTENT: NONE (action {})".format(hfo_env.actionToString(action)))
    if namespace.times_intent_NONE > 5:
      print("Actions_want_check: {0!s}; actions_maybe_check: {1!s}".format(
        ", ".join([hfo_env.actionToString(x) for x in list(actions_want_check)]),
        ", ".join([hfo_env.actionToString(x) for x in list(actions_maybe_check)])))
  sys.stdout.flush()

  prestate_dict = {'action': action,
                   'prestate_bit_list': bit_list,
                   'prestate_self_dict': self_dict,
                   'prestate_goal_dict': goal_dict,
                   'prestate_ball_dict': ball_dict,
                   'checking_intent': True,
                   'intent': None,
                   'namespace': namespace}

  # added: DRIBBLE, PASS, MOVE

  # ends due to out-of-bounds are annoying/noise in the data
  if (action in (hfo.DRIBBLE_TO, hfo.KICK_TO)) or ((action == hfo.MOVE_TO) and (bit_list[4] or bit_list[2])):
    if ball_dict['x_pos'] < -1*POS_ERROR_TOLERANCE:
      x_pos = random.uniform(-0.5,min(0.9,MAX_POS_X_BALL_SAFE))
    elif ball_dict['x_pos'] > POS_ERROR_TOLERANCE:
      x_pos = random.uniform(max(-0.9,MIN_POS_X_BALL_SAFE),0.5)
    else:
      x_pos = random.uniform(max(-0.9,MIN_POS_X_BALL_SAFE),min(0.9,MAX_POS_X_BALL_SAFE))
    if ball_dict['y_pos'] < -1*POS_ERROR_TOLERANCE:
      y_pos = random.uniform(-0.5,min(0.9,MAX_POS_Y_BALL_SAFE))
    elif ball_dict['y_pos'] > POS_ERROR_TOLERANCE:
      y_pos = random.uniform(max(-0.9,MIN_POS_Y_BALL_SAFE),0.5)
    else:
      y_pos = random.uniform(max(-0.9,MIN_POS_Y_BALL_SAFE),min(0.9,MAX_POS_Y_BALL_SAFE))
  else:
    x_pos = random.uniform(MIN_POS_X_OK,MAX_POS_X_OK)
    y_pos = random.uniform(MIN_POS_Y_OK,MAX_POS_Y_OK)


  max_power = 80
  if (action == hfo.KICK) and (get_dist_normalized(ball_dict['x_pos'],
                                                   ball_dict['y_pos'],
                                                   0.0,0.0) > 1):
    if (self_dict['other_angle'] is not None):
      max_power = 100
      direction = self_dict['other_angle']
      if direction > 180:
        direction -= 360
    else:
      max_power = 50
      direction = random.uniform(-180,180)
  else:
    direction = random.uniform(-180,180)

  if action in (hfo.INTERCEPT, hfo.GO_TO_BALL, hfo.DRIBBLE, hfo.MOVE):
    hfo_env.act(save_action_prestate(**prestate_dict))
  elif action is hfo.PASS:
    hfo_env.act(*save_action_prestate(action_params=[namespace.other_dict['unum'].value],
                                      **prestate_dict))
  elif action in (hfo.MOVE_TO, hfo.DRIBBLE_TO):
    hfo_env.act(*save_action_prestate(action_params=[x_pos,y_pos],
                                      **prestate_dict))
  elif action == hfo.KICK_TO:
    if min(abs(x_pos),abs(y_pos)) > 0.5:
      max_speed = 1
    elif max(abs(x_pos),abs(y_pos)) > 0.5:
      max_speed = 2
    else:
      max_speed = 3
    hfo_env.act(*save_action_prestate(action_params=[x_pos,y_pos,
                                                     random.uniform(0,max_speed)],
                                      **prestate_dict))
  elif action == hfo.DASH:
    hfo_env.act(*save_action_prestate(action_params=[random.uniform(-100,100), # power
                                                     direction],
                                      **prestate_dict))
  elif action == hfo.TURN:
    hfo_env.act(*save_action_prestate(action_params=[direction],**prestate_dict))
  elif action == hfo.KICK:
    hfo_env.act(*save_action_prestate(action_params=[random.uniform(0,max_power),
                                                     direction],**prestate_dict))
  else:
    raise RuntimeError("Unknown action {!r}".format(action))

  return None

def run_player2(conf_dir, args, namespace):
  hfo_env2 = hfo.HFOEnvironment()

  if args.seed:
    random.seed(args.seed+1)

  connect2_args = [hfo.HIGH_LEVEL_FEATURE_SET, conf_dir, args.port, "localhost",
                   "base_left", False]

  if args.record:
    connect2_args.append(record_dir=args.rdir)

  time.sleep(5)

  print("Player2 connecting")
  sys.stdout.flush()

  hfo_env2.connectToServer(*connect2_args)

  print("Player2 unum is {0:d}".format(hfo_env2.getUnum()))
  sys.stdout.flush()
  namespace.other_dict['unum'].value = hfo_env2.getUnum()

  status=hfo.IN_GAME
  while status != hfo.SERVER_DOWN:
    if status == hfo.IN_GAME:
      state2 = hfo_env2.getState()
      namespace.other_dict['x_pos'].value = state2[0]
      namespace.other_dict['y_pos'].value = state2[1]
      namespace.other_dict['ball_x_pos'].value = state2[3]
      namespace.other_dict['ball_y_pos'].value = state2[4]
      namespace.other_dict['self_x_pos'].value = state2[13]
      namespace.other_dict['self_y_pos'].value = state2[14]
                                          
      if (abs(state2[0])
          <= 1) and (abs(state2[1])
                     <= 1) and (get_dist_normalized(0.0,
                                                    0.0,
                                                    state2[0],
                                                    state2[1])
                                > POS_ERROR_TOLERANCE):
        hfo_env2.act(hfo.MOVE_TO,0.0,0.0)
      else:
        hfo_env2.act(hfo.TURN,random.uniform(-180,180))
      
      status = hfo_env2.step()

    hfo_env2.act(hfo.NOOP)
    status = hfo_env2.step()

  hfo_env2.act(hfo.QUIT)
  sys.exit(0)


def main_explore_offense_actions_fullstate():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000,
                      help="Server port")
  parser.add_argument('--seed', type=int, default=None,
                      help="Python randomization seed; uses python default if 0 or not given")
  parser.add_argument('--server-seed', type=int, default=None,
                      help="Server randomization seed; uses default if 0 or not given")
  parser.add_argument('--record', default=False, action='store_true',
                      help="Doing HFO --record")
  parser.add_argument('--rdir', type=str, default='log/',
                      help="Set directory to use if doing HFO --record")
  args=parser.parse_args()

  namespace = argparse.Namespace()
  namespace.intent_done = {}
  namespace.intent_done[INTENT_BALL_COLLISION] = 0
  namespace.intent_done[INTENT_BALL_KICKABLE] = 0
  namespace.intent_done[INTENT_GOAL_COLLISION] = 0
  namespace.intent_done[INTENT_PLAYER_COLLISION] = 0
  namespace.action_tried = {}
  namespace.struct_tried = {}
  for n in list(range(hfo.NUM_HFO_ACTIONS)):
    namespace.action_tried[n] = 0
    for x in list(range((2**(BIT_LIST_LEN+1))-1)):
      namespace.struct_tried[struct.pack(STRUCT_PACK, n, x)] = 0
  namespace.other_dict = {'unum': multiprocessing.sharedctypes.Value('B', 0)}
  for name in ['x_pos', 'y_pos', 'ball_x_pos', 'ball_y_pos', 'self_x_pos', 'self_y_pos']:
    namespace.other_dict[name] = multiprocessing.sharedctypes.Value('d', -2.0)

  script_dir   = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))
  binary_dir = os.path.normpath(script_dir + "/../bin")
  conf_dir = os.path.join(binary_dir, 'teams/base/config/formations-dt')
  bin_HFO = os.path.join(binary_dir, "HFO")

  player2_process = multiprocessing.Process(target=run_player2,
                                            kwargs={'conf_dir': conf_dir,
                                                    'args': args,
                                                    'namespace': namespace}
                                            )

  popen_list = [sys.executable, "-x", bin_HFO,
                "--frames-per-trial=3000", "--untouched-time=2000",
                "--port={0:d}".format(args.port),
                "--offense-agents=2", "--defense-npcs=0",
                "--offense-npcs=0", "--trials=20", "--headless",
                "--fullstate"]

  if args.record:
    popen_list.append("--record")
  if args.server_seed:
    popen_list.append("--seed={0:d}".format(args.server_seed))

  HFO_subprocess = subprocess.Popen(popen_list)


  player2_process.daemon = True
  player2_process.start()

  time.sleep(0.2)

  assert (HFO_subprocess.poll() is
          None), "Failed to start HFO with command '{}'".format(" ".join(popen_list))
  assert player2_process.is_alive(), "Failed to start player2 process (exit code {!r})".format(
    player2_process.exitcode)

  if args.seed:
    random.seed(args.seed)

  hfo_env = hfo.HFOEnvironment()

  connect_args = [hfo.LOW_LEVEL_FEATURE_SET, conf_dir, args.port, "localhost",
                  "base_left", False]


  if args.record:
    connect_args.append(record_dir=args.rdir)

  time.sleep(9.8)

  try:
    assert player2_process.is_alive(), "Player2 process exited (exit code {!r})".format(
      player2_process.exitcode)

    print("Main player connecting")
    sys.stdout.flush()

    hfo_env.connectToServer(*connect_args)

    print("Main player unum {0:d}".format(hfo_env.getUnum()))

    for ignored_episode in itertools.count():
      status = hfo.IN_GAME
      namespace.action = hfo.NOOP
      namespace.action_params = []
      namespace.prestate_bit_list = [0,0,0,0,0,0,0]
      namespace.prestate_self_dict = {'x_pos': None,
                                      'y_pos': None,
                                      'body_angle': None,
                                      'other_angle': None,
                                      'vel_rel_angle': None,
                                      'vel_mag': -1.0}
      namespace.prestate_goal_dict = {'dist': get_dist_from_proximity(-1),
                                      'rel_angle': None,
                                      'abs_angle': None,
                                      'x_pos': GOAL_POS_X,
                                      'y_pos': 0.0}
      namespace.prestate_ball_dict = {'dist': get_dist_from_proximity(-1),
                                      'rel_angle': None,
                                      'abs_angle': None,
                                      'x_pos': None,
                                      'y_pos': None,
                                      'vel_rel_angle': None,
                                      'vel_mag': None}
      namespace.intent = None
      namespace.times_intent_NONE = 0
      namespace.checking_intent = True

      while status == hfo.IN_GAME:
        state = hfo_env.getState()

        if namespace.action != hfo.NOOP:
          evaluate_previous_action(hfo_env, state, namespace)
        do_next_action(hfo_env, state, namespace)
        status = hfo_env.step()

      if status == hfo.SERVER_DOWN:
        # summarize results
        hfo_env.act(hfo.QUIT)
        break
  finally:
    if HFO_subprocess.poll() is None:
      HFO_subprocess.terminate()
    os.system("killall -9 rcssserver") # remove if ever start doing multi-server testing!


if __name__ == '__main__':
  main_explore_offense_actions_fullstate()
