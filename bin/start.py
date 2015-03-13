#!/usr/bin/env python
# encoding: utf-8

import subprocess, os, time, numpy, sys
from signal import SIGKILL

# Global list of all/essential running processes
processes, necProcesses = [], []
# Command to run the rcssserver. Edit as needed.
SERVER_CMD = 'rcssserver server::port=6000 server::coach_port=6001 \
server::olcoach_port=6002 server::coach=1 server::game_log_dir=/tmp \
server::text_log_dir=/tmp'
# Command to run the monitor. Edit as needed.
MONITOR_CMD = 'rcssmonitor'

def getAgentDirCmd(name, first):
  """ Returns the team name, command, and directory to run a team. """
  cmd = './start.sh -t %s' % name
  dir = os.path.dirname(os.path.realpath(__file__))
  return name, cmd, dir

def launch(cmd, necessary=True, supressOutput=True, name='Unknown'):
  """Launch a process.

  Appends to list of processes and (optionally) necProcesses if
  necessary flag is True.

  Returns: The launched process.

  """
  kwargs = {}
  if supressOutput:
    kwargs = {'stdout':open('/dev/null','w'),'stderr':open('/dev/null','w')}
  p = subprocess.Popen(cmd.split(' '),shell=False,**kwargs)
  processes.append(p)
  if necessary:
    necProcesses.append([p,name])
  return p

def main(args, team1='left', team2='right', rng=numpy.random.RandomState()):
  """Sets up the teams, launches the server and monitor, starts the
  trainer.
  """
  serverOptions = ''
  if args.sync:
    serverOptions += ' server::synch_mode=on'
  team1, team1Cmd, team1Dir = getAgentDirCmd(team1, True)
  team2, team2Cmd, team2Dir = getAgentDirCmd(team2, False)
  assert os.path.isdir(team1Dir)
  assert os.path.isdir(team2Dir)
  try:
    # Launch the Server
    launch(SERVER_CMD + serverOptions, name='server')
    time.sleep(0.2)
    if not args.headless:
      launch(MONITOR_CMD,name='monitor')
    # Launch the Trainer
    from Trainer import Trainer
    trainer = Trainer(args=args, rng=rng)
    trainer.initComm()
    # Start Team1
    os.chdir(team1Dir)
    launch(team1Cmd,False)
    trainer.waitOnTeam(True) # wait to make sure of team order
    # Start Team2
    os.chdir(team2Dir)
    launch(team2Cmd,False)
    trainer.waitOnTeam(False)
    # Make sure all players are connected
    trainer.checkIfAllPlayersConnected()
    trainer.setTeams()
    # Run HFO
    trainer.run(necProcesses)
  except KeyboardInterrupt:
    print '[start.py] Exiting for CTRL-C'
  finally:
    for p in processes:
      try:
        p.send_signal(SIGKILL)
      except:
        pass
      time.sleep(0.1)

def parseArgs(args=None):
  import argparse
  p = argparse.ArgumentParser(description='Start Half Field Offense.')
  p.add_argument('--headless', dest='headless', action='store_true',
                 help='Run without a monitor')
  p.add_argument('--trials', dest='numTrials', type=int, default=-1,
                 help='Number of trials to run')
  p.add_argument('--frames', dest='numFrames', type=int, default=-1,
                 help='Number of frames to run for')
  p.add_argument('--offense', dest='numOffense', type=int, default=4,
                 help='Number of offensive players')
  p.add_argument('--defense', dest='numDefense', type=int, default=4,
                 help='Number of defensive players')
  p.add_argument('--play-defense', dest='play_offense',
                 action='store_false', default=True,
                 help='Put the learning agent on defensive team')
  p.add_argument('--no-agent', dest='no_agent', action='store_true',
                 help='Don\'t use a learning agent.')
  p.add_argument('--no-sync', dest='sync', action='store_false', default=True,
                 help='Run server in non-sync mode')
  p.add_argument('--server-port', dest='serverPort', type=int, default=6008,
                 help='Port to run agent server on.')
  return p.parse_args(args=args)

if __name__ == '__main__':
  main(parseArgs())
