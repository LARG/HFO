#!/usr/bin/env python3
# encoding: utf-8

from hfo import *
import argparse
import numpy as np
import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'sarsa_libraries','python_wrapper'))
from py_wrapper import *

def getReward(s):
  reward=0
  #--------------------------- 
  if s==GOAL:
    reward=1
  #--------------------------- 
  elif s==CAPTURED_BY_DEFENSE:
    reward=-1
  #--------------------------- 
  elif s==OUT_OF_BOUNDS:
    reward=-1
  #--------------------------- 
  #Cause Unknown Do Nothing
  elif s==OUT_OF_TIME:
    reward=0
  #--------------------------- 
  elif s==IN_GAME:
    reward=0
  #--------------------------- 
  elif s==SERVER_DOWN:  
    reward=0
  #--------------------------- 
  else:
    print("Error: Unknown GameState", s)
  return reward

def purge_features(state):
  st=np.empty(NF,dtype=np.float64)
  stateIndex=0
  tmpIndex= 9 + 3*NOT
  for i in range(len(state)):
    # Ignore first six features and teammate proximity to opponent(when opponent is absent)and opponent features
    if(i < 6 or i>9+6*NOT or (NOO==0 and ((i>9+NOT and i<=9+2*NOT) or i==9)) ): 
      continue;
    #Ignore Angle and Uniform Number of Teammates
    temp =  i-tmpIndex;
    if(temp > 0 and (temp % 3 == 2 or temp % 3 == 0)):
       continue;
    if (i > 9+6*NOT): 
      continue;
    st[stateIndex] = state[i];
    stateIndex+=1;
  return st


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=6000)
  parser.add_argument('--numTeammates', type=int, default=0)
  parser.add_argument('--numOpponents', type=int, default=1)
  parser.add_argument('--numEpisodes', type=int, default=1)
  parser.add_argument('--learnRate', type=float, default=0.1)
  parser.add_argument('--suffix', type=int, default=0)
  args=parser.parse_args()

  # Create the HFO Environment
  hfo = HFOEnvironment()
  #now connect to the server
  hfo.connectToServer(HIGH_LEVEL_FEATURE_SET,'bin/teams/base/config/formations-dt',args.port,'localhost','base_left',False)
  global NF,NA,NOT,NOO
  NOO=args.numOpponents
  if args.numOpponents >0: 
    NF=4+4*args.numTeammates
  else:
    NF=3+3*args.numTeammates
  NOT=args.numTeammates
  NA=NOT+2 #PASS to each teammate, SHOOT, DRIBBLE
  learnR=args.learnRate
  #CMAC parameters
  resolution=0.1
  Range=[2]*NF
  Min=[-1]*NF
  Res=[resolution]*NF
  #Sarsa Agent Parameters
  wt_filename="weights_"+str(NOT+1)+"v"+str(NOO)+'_'+str(args.suffix)
  discFac=1
  Lambda=0
  eps=0.01
  #initialize the function approximator and the sarsa agent
  FA=CMAC(NF, NA, Range, Min, Res)
  SA=SarsaAgent(NF, NA, learnR, eps, Lambda, FA, wt_filename, wt_filename)
 
  #episode rollouts 
  st = np.empty(NF,dtype=np.float64)
  action = -1
  reward = 0
  for episode in range(1,args.numEpisodes+1):
    count=0
    status=IN_GAME
    action=-1

    while status==IN_GAME:
      count=count+1
      # Grab the state features from the environment
      state = hfo.getState()
      if int(state[5])==1:
        if action != -1:
          #print(st)
          reward=getReward(status)
          #fb.SA.update(state,action,reward,discFac)
          SA.update(st,action,reward,discFac)
        st=purge_features(state)
        #take an action
        #action = fb.SA.selectAction(state)
        action = SA.selectAction(st)
        #print("Action:", action)
        if action == 0:
          hfo.act(SHOOT) 
        elif action == 1:
          hfo.act(DRIBBLE)
        else:
          hfo.act(PASS,state[(9+6*NOT)-(action-2)*3]) 
      else:
        hfo.act(MOVE)
        
      status = hfo.step()
    #--------------- end of while loop ------------------------------------------------------
    
    ############# EPISODE ENDS ###################################################################################
    # Check the outcome of the episode
    if action != -1:
      reward=getReward(status)
      SA.update(st, action, reward, discFac)
      SA.endEpisode()
    ############################################################################################################
    # Quit if the server goes down
    if status == SERVER_DOWN:
      hfo.act(QUIT)
      break

   
