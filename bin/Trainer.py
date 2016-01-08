#!/usr/bin/env python
# encoding: utf-8

import sys, numpy, time, os, subprocess
from Communicator import ClientCommunicator, TimeoutError

class DoneError(Exception):
  """ This exception is thrown when the Trainer is finished. """
  def __init__(self,msg='unknown'):
    self.msg = msg
  def __str__(self):
    return 'Done due to %s' % self.msg

class Trainer(object):
  """ Trainer is responsible for setting up the players and game.
  """
  def __init__(self, args, server_port=6001, coach_port=6002):
    self._serverPort = server_port  # The port the server is listening on
    self._coachPort = coach_port # The coach port to talk with the server
    self._logDir = args.logDir # Directory to store logs
    self._record = args.record # Record states + actions
    self._numOffense = args.offenseAgents + args.offenseNPCs # Number offensive players
    self._numDefense = args.defenseAgents + args.defenseNPCs # Number defensive players
    # =============== COUNTERS =============== #
    self._numFrames = 0 # Number of frames seen in HFO trials
    self._numGoalFrames = 0 # Number of frames in goal-scoring HFO trials
    self._frame = 0 # Current frame id
    self._lastTrialStart = -1 # Frame Id in which the last trial started
    # =============== TRIAL RESULTS =============== #
    self._numTrials = 0 # Total number of HFO trials
    self._numGoals = 0 # Trials in which the offense scored a goal
    self._numBallsCaptured = 0 # Trials in which defense captured the ball
    self._numBallsOOB = 0 # Trials in which ball went out of bounds
    self._numOutOfTime = 0 # Trials that ran out of time
    # =============== AGENT =============== #
    self._numAgents = args.offenseAgents + args.defenseAgents
    self._offenseAgents = args.offenseAgents
    self._defenseAgents = args.defenseAgents
    self._agentReady = set([]) # Unums of ready agents
    self._agentTeams = [] # Names of the teams the agents are playing for
    self._agentNumInt = [] # List of agents internal team numbers
    self._agentServerPort = args.port # Base Port for agent's server
    # =============== MISC =============== #
    self._offenseTeamName = '' # Name of the offensive team
    self._defenseTeamName = '' # Name of the defensive team
    self._teams = [] # Team indexes for offensive and defensive teams
    self._isPlaying = False # Is a game being played?
    self._done = False # Are we finished?
    self._agentPopen = [] # Agent's processes
    self._npcPopen = [] # NPC's processes
    self._connectedPlayers = []
    self.initMsgHandlers()

  def launch_npc(self, player_num, play_offense, wait_until_join=True):
    """Launches a player using sample_player binary

    Returns a Popen process object
    """
    print 'Launch npc %s-%d'%(self._offenseTeamName if play_offense
                              else self._defenseTeamName, player_num)
    if play_offense:
      team_name = self._offenseTeamName
    else:
      team_name = self._defenseTeamName
    binary_dir = os.path.dirname(os.path.realpath(__file__))
    config_dir = os.path.join(binary_dir, '../config/formations-dt')
    player_conf = os.path.join(binary_dir, '../config/player.conf')
    player_cmd = os.path.join(binary_dir, 'sample_player')
    player_cmd += ' -t %s -p %i --config_dir %s ' \
                  ' --log_dir %s --player-config %s' \
                  %(team_name, self._serverPort, config_dir,
                    self._logDir, player_conf)
    if self._record:
        player_cmd += ' --record'
    if player_num == 1:
        player_cmd += ' -g'
    kwargs = {'stdout':open('/dev/null', 'w'),
              'stderr':open('/dev/null', 'w')}
    p = subprocess.Popen(player_cmd.split(' '), shell = False, **kwargs)
    if wait_until_join:
      self.waitOnPlayer(player_num, play_offense)
    return p

  def launch_agent(self, agent_num, agent_ext_num, play_offense, port, wait_until_join=True):
    """Launches a learning agent using the agent binary

    Returns a Popen process object
    """
    print 'Launch agent %s-%d'%(self._offenseTeamName if play_offense
                                else self._defenseTeamName, agent_num)
    if play_offense:
      assert self._numOffense > 0
      team_name = self._offenseTeamName
      self._agentTeams.append(team_name)
      # First offense number is reserved for inactive offensive goalie
      internal_player_num = agent_num + 1
      self._agentNumInt.append(internal_player_num)
      numTeammates = self._numOffense - 1
      numOpponents = self._numDefense
    else:
      assert self._numDefense > 0
      team_name = self._defenseTeamName
      self._agentTeams.append(team_name)
      internal_player_num = agent_num
      self._agentNumInt.append(internal_player_num)
      numTeammates = self._numDefense - 1
      numOpponents = self._numOffense
    binary_dir = os.path.dirname(os.path.realpath(__file__))
    config_dir = os.path.join(binary_dir, '../config/formations-dt')
    player_conf = os.path.join(binary_dir, '../config/player.conf')
    agent_cmd =  os.path.join(binary_dir, 'agent')
    agent_cmd += ' -t %s -p %i --numTeammates %i --numOpponents %i' \
                 ' --playingOffense %i --serverPort %i --log_dir %s' \
                 ' --player-config %s --config_dir %s' \
                 %(team_name, self._serverPort, numTeammates,
                   numOpponents, play_offense, port, self._logDir,
                   player_conf, config_dir)
    if agent_ext_num == 1:
      agent_cmd += ' -g'
    if self._record:
      agent_cmd += ' --record'
    # Comment next two lines to show output from agent.cpp and the server
    kwargs = {'stdout':open('/dev/null', 'w'),
              'stderr':open('/dev/null', 'w')}
    p = subprocess.Popen(agent_cmd.split(' '), shell = False, **kwargs)
    if wait_until_join:
      self.waitOnPlayer(agent_ext_num, play_offense)
    return p

  def getDefensiveRoster(self, team_name):
    """Returns a list of player numbers on a given team that are thought
    to prefer defense. This map is not set in stone as the players on
    some teams can adapt and change their roles.

    """
    if team_name == 'Borregos':
      return [9,10,8,11,7,4,6,2,3,5]
    elif team_name == 'WrightEagle':
      return [5,2,8,9,10,6,3,11,4,7]
    else:
      return [2,3,4,5,6,7,8,11,9,10]

  def getOffensiveRoster(self, team_name):
    """Returns a list of player numbers on a given team that are thought
    to prefer offense. This map is not set in stone as the players on
    some teams can adapt and change their roles.

    """
    if team_name == 'Borregos':
      return [2,4,6,5,3,7,9,10,8,11]
    elif team_name == 'WrightEagle':
      return [11,4,7,3,6,10,8,9,2,5]
    else:
      return [11,7,8,9,10,6,3,2,4,5]

  def addTeam(self, team_name):
    """ Adds a team to the team list"""
    self._teams.append(team_name)

  def setTeams(self):
    """ Sets the offensive and defensive teams and player rosters. """
    self._offenseTeamInd = 0
    self._offenseTeamName = self._teams[self._offenseTeamInd]
    self._defenseTeamName = self._teams[1-self._offenseTeamInd]
    offensive_roster = self.getOffensiveRoster(self._offenseTeamName)
    defensive_roster = self.getDefensiveRoster(self._defenseTeamName)
    self._offenseOrder = [1] + offensive_roster # 1 for goalie
    self._defenseOrder = [1] + defensive_roster # 1 for goalie

  def teamToInd(self, team_name):
    """ Returns the index of a given team. """
    return self._teams.index(team_name)

  def parseMsg(self, msg):
    """ Parse a message """
    assert(msg[0] == '(')
    res, ind = self.__parseMsg(msg,1)
    assert(ind == len(msg)), msg
    return res

  def __parseMsg(self, msg, ind):
    """ Recursively parse a message. """
    res = []
    while True:
      if msg[ind] == '"':
        ind += 1
        startInd = ind
        while msg[ind] != '"':
          ind += 1
        res.append(msg[startInd:ind])
        ind += 1
      elif msg[ind] == '(':
        inner,ind = self.__parseMsg(msg,ind+1)
        res.append(inner)
      elif msg[ind] == ' ':
        ind += 1
      elif msg[ind] == ')':
        return res,ind+1
      else:
        startInd = ind
        while msg[ind] not in ' ()':
          ind += 1
        res.append(msg[startInd:ind])

  def initComm(self):
    """ Initialize communication to server. """
    self._comm = ClientCommunicator(port=self._coachPort)
    self.send('(init (version 8.0))')
    self.checkMsg('(init ok)', retryCount=5)
    # self.send('(eye on)')
    self.send('(ear on)')

  def _hearRef(self, body):
    """ Handles hear messages from referee. """
    assert body[0] == 'referee', 'Expected referee message.'
    _,ts,event = body
    self._frame = int(ts)
    if event == 'GOAL':
      self._numGoals += 1
      self._numGoalFrames += self._frame - self._lastTrialStart
    elif event == 'OUT_OF_BOUNDS':
      self._numBallsOOB += 1
    elif event == 'CAPTURED_BY_DEFENSE':
      self._numBallsCaptured += 1
    elif event == 'OUT_OF_TIME':
      self._numOutOfTime += 1
    elif event == 'HFO_FINISHED':
      self._done = True
    if event in {'GOAL','OUT_OF_BOUNDS','CAPTURED_BY_DEFENSE','OUT_OF_TIME'}:
      self._numTrials += 1
      print 'EndOfTrial: %d / %d %d %s'%\
        (self._numGoals, self._numTrials, self._frame, event)
      self._numFrames += self._frame - self._lastTrialStart
      self._lastTrialStart = self._frame

  def _hear(self, body):
    """ Handle a hear message. """
    if body[0] == 'referee':
      self._hearRef(body)
      return
    timestep,playerInfo,msg = body
    try:
      _,team,player = playerInfo[:3]
      length = int(msg[0])
    except:
      return
    msg = msg[1:length+1]
    if msg == 'START':
      if self._isPlaying:
        print 'Already playing, ignoring message'
      else:
        self.startGame()
    elif msg == 'DONE':
      raise DoneError
    elif msg == 'ready':
      print 'Agent Connected:', team, player
      self._agentReady.add((team, player))
    else:
      print 'Unhandled message from agent: %s' % msg

  def initMsgHandlers(self):
    """ Create handlers for different messages. """
    self._msgHandlers = []
    self.ignoreMsg('player_param')
    self.ignoreMsg('player_type')
    self.ignoreMsg('ok','change_mode')
    self.ignoreMsg('ok','ear')
    self.ignoreMsg('ok','move')
    self.ignoreMsg('ok','recover')
    self.ignoreMsg('ok','say')
    self.ignoreMsg('server_param')
    self.registerMsgHandler(self._hear,'hear')

  def recv(self, retryCount=None):
    """ Recieve a message. Retry a specified number of times. """
    return self._comm.recvMsg(retryCount=retryCount).strip()

  def send(self, msg):
    """ Send a message. """
    self._comm.sendMsg(msg)

  def checkMsg(self, expectedMsg, retryCount=None):
    """ Check that the next message is same as expected message. """
    msg = self.recv(retryCount)
    if msg != expectedMsg:
      print >>sys.stderr,'Error with message'
      print >>sys.stderr,'  expected: %s' % expectedMsg
      print >>sys.stderr,'  received: %s' % msg
      # print >>sys.stderr,len(expectedMsg),len(msg)
      raise ValueError

  def convertToExtPlayer(self, team, num):
    """ Returns the external player number for a given player. """
    assert team == self._offenseTeamName or team == self._defenseTeamName,\
      'Invalid team name %s. Valid choices: %s, %s.'\
      %(team, self._offenseTeamName, self._defenseTeamName)
    if team == self._offenseTeamName:
      return self._offenseOrder[num]
    else:
      return self._defenseOrder[num]

  def registerMsgHandler(self,handler,*args,**kwargs):
    '''Register a message handler.

    Handler will be called on a message that matches *args.

    '''
    args = list(args)
    i,_,_ = self._findHandlerInd(args)
    if i < 0:
      self._msgHandlers.append([args,handler])
    else:
      if ('quiet' not in kwargs) or (not kwargs['quiet']):
        print 'Updating handler for %s' % (' '.join(args))
      self._msgHandlers[i] = [args,handler]

  def unregisterMsgHandler(self, *args):
    """ Delete a message handler. """
    i,_,_ = self._findHandlerInd(args)
    assert(i >= 0)
    del self._msgHandlers[i]

  def _findHandlerInd(self, msg):
    """ Find the handler for a particular message. """
    msg = list(msg)
    for i,(partial,handler) in enumerate(self._msgHandlers):
      recPartial = msg[:len(partial)]
      if recPartial == partial:
        return i,len(partial),handler
    return -1,None,None

  def handleMsg(self, msg):
    """ Handle a message using the registered handlers. """
    i,prefixLength,handler = self._findHandlerInd(msg)
    if i < 0:
      print 'Unhandled message:',msg[0:2]
    else:
      handler(msg[prefixLength:])

  def ignoreMsg(self,*args,**kwargs):
    """ Ignore a certain type of message. """
    self.registerMsgHandler(lambda x: None,*args,**kwargs)

  def listenAndProcess(self, retry_count=None):
    """ Gather messages and process them. """
    try:
      msg = self.recv(retry_count)
      assert((msg[0] == '(') and (msg[-1] == ')')),'|%s|' % msg
      msg = self.parseMsg(msg)
      self.handleMsg(msg)
    except TimeoutError:
      pass

  def disconnectPlayer(self, player, player_num, on_offense):
    """Wait on a launched player to disconnect from the server. """
    # print 'Disconnect %s-%d'%(self._offenseTeamName if on_offense
    #                           else self._defenseTeamName, player_num)
    team_name = self._offenseTeamName if on_offense else self._defenseTeamName
    self.send('(disconnect_player %s %d)'%(team_name, player_num))
    player.kill()

  def waitOnPlayer(self, player_num, on_offense):
    """Wait on a launched player to connect and be reported by the
    server.

    """
    self.send('(look)')
    partial = ['ok','look']
    self._numPlayers = 0
    def f(body):
      del self._connectedPlayers[:]
      for i in xrange(4, len(body)):
        _,team,num = body[i][0][:3]
        if (team, num) not in self._connectedPlayers:
          self._connectedPlayers.append((team,num))
    self.registerMsgHandler(f,*partial,quiet=True)
    team_name = self._offenseTeamName if on_offense else self._defenseTeamName
    while (team_name, str(player_num)) not in self._connectedPlayers:
      self.listenAndProcess()
      self.send('(look)')
    self.ignoreMsg(*partial,quiet=True)

  def checkIfAllPlayersConnected(self):
    """ Returns true if all players are connected. """
    print 'Checking all players are connected'
    self.send('(look)')
    partial = ['ok','look']
    self._numPlayers = 0
    def f(x):
      self._numPlayers = len(x) - 4 # -4 for time, ball, goal_l, and goal_r
      self.send('(look)')
    self.registerMsgHandler(f,*partial,quiet=True)
    while self._numPlayers != self._numOffense + self._numDefense:
      self.listenAndProcess()
    self.ignoreMsg(*partial,quiet=True)

  def startGame(self):
    """ Starts a game of HFO. """
    self.send('(change_mode play_on)')
    self._isPlaying = True

  def printStats(self):
    print 'TotalFrames = %i, AvgFramesPerTrial = %.1f, AvgFramesPerGoal = %.1f'\
      %(self._numFrames,
        self._numFrames / float(self._numTrials) if self._numTrials > 0 else float('nan'),
        self._numGoalFrames / float(self._numGoals) if self._numGoals > 0 else float('nan'))
    print 'Trials             : %i' % self._numTrials
    print 'Goals              : %i' % self._numGoals
    print 'Defense Captured   : %i' % self._numBallsCaptured
    print 'Balls Out of Bounds: %i' % self._numBallsOOB
    print 'Out of Time        : %i' % self._numOutOfTime

  def checkLive(self, necProcesses):
    """Returns true if each of the necessary processes is still alive and
    running.

    """
    for p,name in necProcesses:
      if p.poll() is not None:
        print 'Something necessary closed (%s), exiting' % name
        return False
    return True

  def run(self, necProcesses):
    """ Run the trainer """
    try:
      self.setTeams()
      offense_unums = self._offenseOrder[1: self._numOffense + 1]
      sorted_offense_agent_unums = sorted(self._offenseOrder[1:self._offenseAgents+1])
      defense_unums = self._defenseOrder[: self._numDefense]
      sorted_defense_agent_unums = sorted(self._defenseOrder[:self._defenseAgents])

      # Launch offense
      agent_num = 0
      for player_num in xrange(1, 12):
        if agent_num < self._offenseAgents and player_num == sorted_offense_agent_unums[agent_num]:
          port = self._agentServerPort + agent_num
          agent = self.launch_agent(agent_num, sorted_offense_agent_unums[agent_num],
                                    play_offense=True, port=port)
          self._agentPopen.append(agent)
          necProcesses.append([agent, 'offense_agent_' + str(agent_num)])
          agent_num += 1
        else:
          player = self.launch_npc(player_num, play_offense=True)
          if player_num in offense_unums:
            self._npcPopen.append(player)
            necProcesses.append([player, 'offense_npc_' + str(player_num)])
          else:
            self.disconnectPlayer(player, player_num, on_offense=True)

      # Launch defense
      agent_num = 0
      for player_num in xrange(1, 12):
        if agent_num < self._defenseAgents and player_num == sorted_defense_agent_unums[agent_num]:
          port = self._agentServerPort + agent_num + self._offenseAgents
          agent = self.launch_agent(agent_num, sorted_defense_agent_unums[agent_num],
                                    play_offense=False, port=port)
          self._agentPopen.append(agent)
          necProcesses.append([agent, 'defense_agent_' + str(agent_num)])
          agent_num += 1
        else:
          player = self.launch_npc(player_num, play_offense=False)
          if player_num in defense_unums:
            self._npcPopen.append(player)
            necProcesses.append([player, 'defense_npc_' + str(player_num)])
          else:
            self.disconnectPlayer(player, player_num, on_offense=False)

      self.checkIfAllPlayersConnected()

      if self._numAgents > 0:
        print 'Agents awaiting your connections'
        necOff = set([(self._offenseTeamName,str(x)) for x in sorted_offense_agent_unums])
        necDef = set([(self._defenseTeamName,str(x)) for x in sorted_defense_agent_unums])
        necAgents = necOff.union(necDef)
        while self.checkLive(necProcesses) and self._agentReady != necAgents:
          self.listenAndProcess()

      # Broadcast the HFO configuration
      offense_nums = ' '.join([str(self.convertToExtPlayer(self._offenseTeamName, i))
                               for i in xrange(1, self._numOffense + 1)])
      defense_nums = ' '.join([str(self.convertToExtPlayer(self._defenseTeamName, i))
                               for i in xrange(self._numDefense)])
      self.send('(say HFO_SETUP offense_name %s defense_name %s num_offense %d'\
                  ' num_defense %d offense_nums %s defense_nums %s)'
                %(self._offenseTeamName, self._defenseTeamName,
                  self._numOffense, self._numDefense,
                  offense_nums, defense_nums))
      print 'Starting game'
      self.startGame()
      while self.checkLive(necProcesses) and not self._done:
        prevFrame = self._frame
        self.listenAndProcess()
    except TimeoutError:
      print 'Haven\'t heard from the server for too long, Exiting'
    except (KeyboardInterrupt, DoneError):
      print 'Finished'
    finally:
      try:
        self._comm.sendMsg('(bye)')
      except:
        pass
      for p in self._agentPopen:
        try:
          p.terminate()
          time.sleep(0.1)
          p.kill()
        except:
          pass
      for p in self._npcPopen:
        try:
          p.terminate()
          time.sleep(0.1)
          p.kill()
        except:
          pass
      self._comm.close()
      self.printStats()
