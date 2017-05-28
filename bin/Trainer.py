#!/usr/bin/env python
# encoding: utf-8

import sys, numpy, time, os, subprocess, Teams
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
    self._agentPlayGoalie = args.agentPlayGoalie
    self._agentReady = set([]) # Unums of ready agents
    self._agentTeams = [] # Names of the teams the agents are playing for
    self._agentNumInt = [] # List of agents internal team numbers
    self._agentServerPort = args.port # Base Port for agent's server
    # =============== MISC =============== #
    self._offenseTeamName = '' # Name of the offensive team
    self._defenseTeamName = '' # Name of the defensive team
    self._isPlaying = False # Is a game being played?
    self._done = False # Are we finished?
    self._agentPopen = [] # Agent's processes
    self._npcPopen = [] # NPC's processes
    self._connectedPlayers = [] # List of connected players
    self.initMsgHandlers()

  def launch_agent(self, agent_num, agent_ext_num, play_offense, port, wait_until_join=True):
    """Launches a learning agent using the agent binary

    Returns a Popen process object
    """
    if play_offense:
      assert self._numOffense > 0
      team_name = self._offenseTeamName
      self._agentTeams.append(team_name)
      # First offense number is reserved for inactive offensive goalie
      internal_player_num = agent_num + 1
      self._agentNumInt.append(internal_player_num)
    else:
      assert self._numDefense > 0
      team_name = self._defenseTeamName
      self._agentTeams.append(team_name)
      internal_player_num = agent_num
      self._agentNumInt.append(internal_player_num)
    binary_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                              'teams', 'base')
    config_dir = os.path.join(binary_dir, 'config/formations-dt')
    print(("Waiting for player-controlled agent %s-%d: config_dir=%s, "\
          "server_port=%d, server_addr=%s, team_name=%s, play_goalie=%r"
          % (self._offenseTeamName if play_offense else self._defenseTeamName,
             agent_num, config_dir, self._serverPort, "localhost", team_name,
             agent_ext_num==1)))
    if wait_until_join:
      self.waitOnPlayer(agent_ext_num, play_offense)
    return None

  def createTeam(self, requested_team_name, play_offense):
    """ Given a team name, returns the team object. """
    teams_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'teams')
    if requested_team_name == 'helios':
      print('Creating team Helios')
      team_name = 'HELIOS_' + ('left' if play_offense else 'right')
      team_dir = os.path.join(teams_dir, 'helios', 'helios-13Eindhoven')
      lib_dir = os.path.join(teams_dir, 'helios', 'local', 'lib')
      return Teams.Helios(team_name, team_dir, lib_dir,
                          binaryName='helios_player', host='localhost',
                          port=self._serverPort)
    elif requested_team_name == 'base':
      print('Creating team Agent2d (base)')
      team_name = 'base_' + ('left' if play_offense else 'right')
      team_dir = os.path.join(teams_dir, 'base')
      lib_dir = None
      return Teams.Agent2d(team_name, team_dir, lib_dir,
                           binaryName='sample_player', logDir=self._logDir,
                           record=self._record, host='localhost',
                           port=self._serverPort)
    else:
      print('Unknown team requested: ' + requested_team_name)
      sys.exit(1)

  def getTeams(self, offense_team_name, defense_team_name):
    """ Sets the offensive and defensive teams and player rosters. """
    # Set up offense team
    offenseTeam = self.createTeam(offense_team_name, play_offense=True)
    self._offenseTeamName = offenseTeam._name
    self._offenseOrder = [1] + offenseTeam._offense_order # 1 for goalie
    # Set up defense team
    defenseTeam = self.createTeam(defense_team_name, play_offense=False)
    self._defenseTeamName = defenseTeam._name
    self._defenseOrder = [1] + defenseTeam._defense_order # 1 for goalie
    return (offenseTeam, defenseTeam)

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
    endOfTrial = False
    if 'GOAL' in event:
      self._numGoals += 1
      self._numGoalFrames += self._frame - self._lastTrialStart
      endOfTrial = True
    elif event == 'OUT_OF_BOUNDS':
      self._numBallsOOB += 1
      endOfTrial = True
    elif 'CAPTURED_BY_DEFENSE' in event:
      self._numBallsCaptured += 1
      endOfTrial = True
    elif event == 'OUT_OF_TIME':
      self._numOutOfTime += 1
      endOfTrial = True
    elif event == 'HFO_FINISHED':
      self._done = True
    if endOfTrial:
      self._numTrials += 1
      print('EndOfTrial: %d / %d %d %s'%\
        (self._numGoals, self._numTrials, self._frame, event))
      self._numFrames += self._frame - self._lastTrialStart
      self._lastTrialStart = self._frame
      self.getConnectedPlayers()

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
        print('Already playing, ignoring message')
      else:
        self.startGame()
    elif msg == 'DONE':
      raise DoneError
    elif msg == 'ready':
      print('Agent Connected:', team, player)
      self._agentReady.add((team, player))
    else:
      print('Unhandled message from agent: %s' % msg)

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
      sys.stderr.write('Error with message')
      sys.stderr.write('  expected: ' + expectedMsg)
      sys.stderr.write('  received: ' + msg)
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
        print('Updating handler for %s' % (' '.join(args)))
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
      print('Unhandled message:',msg[0:2])
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
    team_name = self._offenseTeamName if on_offense else self._defenseTeamName
    while (team_name, str(player_num)) in self._connectedPlayers:
      self.send('(disconnect_player %s %d)'%(team_name, player_num))
      self.getConnectedPlayers()
    player.kill()

  def getConnectedPlayers(self):
    """ Get the list of connected players. Populates self._connectedPlayers. """
    self._gotLook = False
    self.send('(look)')
    partial = ['ok','look']
    def f(body):
      self._gotLook = True
      del self._connectedPlayers[:]
      for i in range(4, len(body)):
        _,team,num = body[i][0][:3]
        if (team, num) not in self._connectedPlayers:
          self._connectedPlayers.append((team,num))
    self.registerMsgHandler(f,*partial,quiet=True)
    while not self._gotLook:
      self.listenAndProcess()
      self.send('(look)')
    self.ignoreMsg(*partial,quiet=True)

  def waitOnPlayer(self, player_num, on_offense):
    """ Wait on a launched player to connect and be reported by the server. """
    team_name = self._offenseTeamName if on_offense else self._defenseTeamName
    while (team_name, str(player_num)) not in self._connectedPlayers:
      self.getConnectedPlayers()

  def allPlayersConnected(self):
    """ Returns true all players are connected. """
    return len(self._connectedPlayers) == self._numOffense + self._numDefense

  def sendHFOConfig(self):
    """ Broadcast the HFO configuration """
    offense_nums = ' '.join([str(self.convertToExtPlayer(self._offenseTeamName, i))
                             for i in range(1, self._numOffense + 1)])
    defense_nums = ' '.join([str(self.convertToExtPlayer(self._defenseTeamName, i))
                             for i in range(self._numDefense)])
    self.send('(say HFO_SETUP offense_name %s defense_name %s num_offense %d'\
                ' num_defense %d offense_nums %s defense_nums %s)'
              %(self._offenseTeamName, self._defenseTeamName,
                self._numOffense, self._numDefense,
                offense_nums, defense_nums))

  def startGame(self):
    """ Starts a game of HFO. """
    self.send('(change_mode play_on)')
    self._isPlaying = True

  def printStats(self):
    print('TotalFrames = %i, AvgFramesPerTrial = %.1f, AvgFramesPerGoal = %.1f'\
      %(self._numFrames,
        self._numFrames / float(self._numTrials) if self._numTrials > 0 else float('nan'),
        self._numGoalFrames / float(self._numGoals) if self._numGoals > 0 else float('nan')))
    print('Trials             : %i' % self._numTrials)
    print('Goals              : %i' % self._numGoals)
    print('Defense Captured   : %i' % self._numBallsCaptured)
    print('Balls Out of Bounds: %i' % self._numBallsOOB)
    print('Out of Time        : %i' % self._numOutOfTime)

  def checkLive(self, necProcesses):
    """Returns true if each of the necessary processes is still alive and
    running.

    """
    for p,name in necProcesses:
      if p is not None and p.poll() is not None:
        print('Something necessary closed (%s), exiting' % name)
        return False
    return True

  def run(self, necProcesses, offense_team_name, defense_team_name):
    """ Run the trainer """
    try:
      (offenseTeam, defenseTeam) = self.getTeams(offense_team_name, defense_team_name)
      offense_unums = self._offenseOrder[1: self._numOffense + 1]
      sorted_offense_agent_unums = sorted(self._offenseOrder[1:self._offenseAgents+1])
      defense_unums = sorted(self._defenseOrder[: self._numDefense])
      sorted_defense_agent_unums = \
        defense_unums[:self._defenseAgents] if self._agentPlayGoalie \
        else defense_unums[-self._defenseAgents:]

      # Launch offense
      agent_num = 0
      for player_num in range(1, 12):
        if agent_num < self._offenseAgents and player_num == sorted_offense_agent_unums[agent_num]:
          port = self._agentServerPort + agent_num
          agent = self.launch_agent(agent_num, sorted_offense_agent_unums[agent_num],
                                    play_offense=True, port=port)
          self._agentPopen.append(agent)
          necProcesses.append([agent, 'offense_agent_' + str(agent_num)])
          agent_num += 1
        else:
          player = offenseTeam.launch_npc(player_num)
          self.waitOnPlayer(player_num, True)
          if player_num in offense_unums:
            self._npcPopen.append(player)
            necProcesses.append([player, 'offense_npc_' + str(player_num)])
          else:
            self.disconnectPlayer(player, player_num, on_offense=True)

      # Launch defense
      agent_num = 0
      for player_num in range(1, 12):
        if agent_num < self._defenseAgents and player_num == sorted_defense_agent_unums[agent_num]:
          port = self._agentServerPort + agent_num + self._offenseAgents
          agent = self.launch_agent(agent_num, sorted_defense_agent_unums[agent_num],
                                    play_offense=False, port=port)
          self._agentPopen.append(agent)
          necProcesses.append([agent, 'defense_agent_' + str(agent_num)])
          agent_num += 1
        else:
          player = defenseTeam.launch_npc(player_num)
          self.waitOnPlayer(player_num, False)
          if player_num in defense_unums:
            self._npcPopen.append(player)
            necProcesses.append([player, 'defense_npc_' + str(player_num)])
          else:
            self.disconnectPlayer(player, player_num, on_offense=False)

      print('Checking all players connected')
      while not self.allPlayersConnected():
        self.getConnectedPlayers()

      time.sleep(0.1)
      self.sendHFOConfig()

      print('Starting game')
      time.sleep(0.1)
      self.startGame()
      while self.allPlayersConnected() and self.checkLive(necProcesses) and not self._done:
        prevFrame = self._frame
        self.listenAndProcess()
    except TimeoutError:
      print('Haven\'t heard from the server for too long, Exiting')
    except (KeyboardInterrupt, DoneError):
      print('Finished')
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
