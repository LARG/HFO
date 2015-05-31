#!/usr/bin/env python
# encoding: utf-8

import subprocess, os, time, numpy, sys
from signal import SIGKILL

# Global list of all/essential running processes
processes, necProcesses = [], []
# Command to run the rcssserver. Edit as needed.
SERVER_CMD = 'rcssserver'
# Command to run the monitor. Edit as needed.
MONITOR_CMD = 'rcssmonitor'

def getAgentDirCmd(binary_dir, teamname, server_port=6000, coach_port=6002,
                   logDir='log', record=False):
  """ Returns the team name, command, and directory to run a team. """
  cmd = 'start.sh -t %s -p %i -P %i --log-dir %s'%(teamname, server_port,
                                                   coach_port, logDir)
  if record:
    cmd += ' --record'
  cmd = os.path.join(binary_dir, cmd)
  return teamname, cmd

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
  if not os.path.exists(args.logDir):
    os.makedirs(args.logDir)
  binary_dir   = os.path.dirname(os.path.realpath(__file__))
  server_port  = args.port + 1
  coach_port   = args.port + 2
  olcoach_port = args.port + 3
  serverOptions = ' server::port=%i server::coach_port=%i ' \
                  'server::olcoach_port=%i server::coach=1 ' \
                  'server::game_log_dir=%s server::text_log_dir=%s' \
                  %(server_port, coach_port, olcoach_port,
                    args.logDir, args.logDir)
  if args.sync:
    serverOptions += ' server::synch_mode=on'
  team1, team1Cmd = getAgentDirCmd(binary_dir, team1, server_port, coach_port,
                                   args.logDir, args.record)
  team2, team2Cmd = getAgentDirCmd(binary_dir, team2, server_port, coach_port,
                                   args.logDir, args.record)
  try:
    # Launch the Server
    server = launch(SERVER_CMD + serverOptions, name='server')
    time.sleep(0.2)
    assert server.poll() is None,\
      'Failed to launch Server with command: \"%s\"'%(SERVER_CMD)
    if not args.headless:
      monitorOptions = ' --port=%i'%(server_port)
      launch(MONITOR_CMD + monitorOptions, name='monitor')
    # Launch the Trainer
    from Trainer import Trainer
    trainer = Trainer(args=args, rng=rng)
    trainer.initComm()
    # Start Team1
    launch(team1Cmd,False)
    trainer.waitOnTeam(True) # wait to make sure of team order
    # Start Team2
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
  p.add_argument('--port', dest='port', type=int, default=6000,
                 help='Agent server\'s port. rcssserver, coach, and ol_coach'\
                 ' will be incrementally allocated the following ports.')
  p.add_argument('--log-dir', dest='logDir', default='log/',
                 help='Directory to store logs.')
  p.add_argument('--record', dest='record', action='store_true',
                 help='Record logs of states and actions.')
  p.add_argument('--agent-on-ball', dest='agent_on_ball', action='store_true',
                 help='Agent starts with the ball.')
  return p.parse_args(args=args)

if __name__ == '__main__':
  main(parseArgs())
