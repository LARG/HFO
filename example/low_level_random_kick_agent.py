#!/usr/bin/env python3
# encoding: utf-8

from hfo import *
import argparse
import numpy as np
import math as m
import sys, os
import itertools

def rad_to_deg(rad):
  return rad/m.pi*180
def sign(x):
  return (int(x>=0)-0.5)*2

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000, help="Server port")
  args=parser.parse_args()

  hfo = HFOEnvironment()
  hfo.connectToServer(LOW_LEVEL_FEATURE_SET,
    '/bin/teams/base/config/formations-dt',
    args.port,'localhost','base_left',False)

  States, Actions, Statuses = [], [], []

  for episode in itertools.count():
    status=IN_GAME
    while status==IN_GAME:
      state = hfo.getState()

      if int(state[12]) == 1: # Kickable = 1
        goal_center_angle = rad_to_deg(m.acos(state[14])) * sign(m.asin(state[13]))
        # turn to goal center
        if abs(goal_center_angle) > 45:
          hfo.act(1, goal_center_angle)
        # kick
        else:
          power = np.random.uniform(0,100)
          hfo.act(3, power, goal_center_angle)
      else: # Kickable = -1
        ball_angle = rad_to_deg(m.acos(state[52])) * sign(m.asin(state[51]))
        # turn to ball
        if abs(ball_angle) > 10:
          hfo.act(1, ball_angle)
        # go to ball
        else:
          power = np.random.uniform(0,100)
          hfo.act(0, power, ball_angle)
        
      status = hfo.step()
    #--------------- end of while loop ------------------------------------------------------

    # Quit if the server goes down
    if status == SERVER_DOWN:
      hfo.act(QUIT)
      break
