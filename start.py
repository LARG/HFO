#!/usr/bin/env python
# encoding: utf-8

import subprocess, os, time, numpy
from signal import SIGINT

UT_AGENT_DIR    = '/u/mhauskn/projects/robocup_libs/utvillasim/agent2d-3.1.1/src/'
OTHER_AGENT_DIR = '/projects/agents2/villasim/opponents2D/'
#'/u/mhauskn/projects/robocup_libs/utvillasim/agent2d-3.1.1/src/'

SERVER_CMD = 'rcssserver server::port=6000 server::coach_port=6001 server::olcoach_port=6002 server::coach=1 server::game_log_dir=/tmp server::text_log_dir=/tmp'
MONITOR_CMD = 'rcssmonitor'

def getAgentDirCmd(name,first):
  if name == 'ut':
    if first:
      name = 'ut1'
    else:
      name = 'ut2'
    cmd = './start.sh -t %s' % name
    dir = UT_AGENT_DIR
  elif name == 'base':
    dir = os.path.join(OTHER_AGENT_DIR,name,'src')
    if first:
      name = 'base1'
    else:
      name = 'base2'
    cmd = './start.sh -t %s' % name
  else:
    cmd = './start.sh'
    dir = os.path.join(OTHER_AGENT_DIR,name)
  return name,cmd,dir

#team2 = 'oxsy' # fcportugal2d, gdut-tiji, marlik, nadco-2d, warthog


processes = []
necProcesses = []

def launch(cmd,necessary=True,supressOutput=True,name='Unknown'):
  kwargs = {}
  if supressOutput:
    kwargs = {'stdout':open('/dev/null','w'),'stderr':open('/dev/null','w')}
  p = subprocess.Popen(cmd.split(' '),shell=False,**kwargs)
  processes.append(p)
  if necessary:
    necProcesses.append([p,name])
  return p

def main(team1,team2,rng,options):
  serverOptions = ''
  if options.sync:
    serverOptions += ' server::synch_mode=on'
    
  team1,team1Cmd,team1Dir = getAgentDirCmd(team1,True)
  team2,team2Cmd,team2Dir = getAgentDirCmd(team2,False)
  if not os.path.isdir(team1Dir):
    print 'Bad team 1: %s' % team1
    sys.exit(1)
  if not os.path.isdir(team2Dir):
    print 'Bad team 2: %s' % team2
    sys.exit(1)
  try:
    launch(SERVER_CMD + serverOptions,name='server')
    time.sleep(0.2)
    if not options.headless:
      launch(MONITOR_CMD,name='monitor')
    
    # launch trainer
    from Trainer import Trainer
    seed = rng.randint(numpy.iinfo('i').max)
    trainer = Trainer(seed=seed,options=options)
    trainer.initComm()
    # start team 1
    os.chdir(team1Dir)
    launch(team1Cmd,False)
    trainer.waitOnTeam(True) # wait to make sure of team order
    # start team 2
    os.chdir(team2Dir)
    launch(team2Cmd,False)
    trainer.waitOnTeam(False)
    # make sure all players are connected
    trainer.checkIfAllPlayersConnected()
    trainer.setTeams()
    # run
    trainer.run(necProcesses)
  except KeyboardInterrupt:
    print 'Exiting for CTRL-C'
  finally:
    for p in processes:
      try:
        p.send_signal(SIGINT)
      except:
        pass
    #print 'Done killing children (hopefully)'
    time.sleep(0.1)

if __name__ == '__main__':
  import sys
  
  from optparse import OptionParser

  p = OptionParser('''Usage: ./startHFO.py [team1 [team2]]
  teams are ut or the ones in the agents directory''')
  p.add_option('-s','--no-sync',dest='sync',action='store_false',default=True,help='run server in non-sync mode')
  p.add_option('--headless',dest='headless',action='store_true',default=False,help='run in headless mode')
  p.add_option('-a','--adhoc',dest='useAdhoc',action='store_true',default=False,help='use an adhoc agent')
  p.add_option('-d','--adhocDefense',dest='adhocOffense',action='store_false',default=True,help='put the ad hoc agent on defense')
  p.add_option('-n','--numTrials',dest='numTrials',action='store',type='int',default=-1,help='number of trials to run')
  p.add_option('-f','--frames',dest='numFrames',action='store',type='int',default=-1,help='number of frames to run for')
  p.add_option('--offense',dest='numOffense',action='store',type='int',default=4,help='number of offensive players')
  p.add_option('--defense',dest='numDefense',action='store',type='int',default=4,help='number of defensive players (excluding the goalie)')
  p.add_option('--learn-actions',dest='numLearnActions',action='store',type='int',default=0,help='number of instances to learn actions instead of the regular behavior')

  options,args = p.parse_args()
  if len(args) > 2:
    print 'Incorrect number of arguments'
    p.parse_args(['--help'])
    sys.exit(2)

  options.learnActions = (options.numLearnActions > 0)
    
  team1 = 'ut'
  team2 = 'ut'

  if len(args) >= 1:
    team1 = args[0]
    if len(args) >= 2:
      team2 = args[1]
  seed = int(time.time())
  rng = numpy.random.RandomState(seed)

  main(team1,team2,rng,options)
