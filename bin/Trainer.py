#!/usr/bin/env python
# encoding: utf-8

import sys, numpy, time, os, subprocess, re
from signal import SIGINT
from Communicator import ClientCommunicator, TimeoutError

class DoneError(Exception):
  """ This exception is thrown when the Trainer is finished. """
  def __init__(self,msg='unknown'):
    self.msg = msg
  def __str__(self):
    return 'Done due to %s' % self.msg

class DummyPopen(object):
  """ Emulates a Popen object without actually starting a process. """
  def __init__(self, pid):
    self.pid = pid
  def poll(self):
    try:
      os.kill(self.pid, 0)
      return None
    except OSError:
      return 0
  def send_signal(self, sig):
    try:
      os.kill(self.pid, sig)
    except OSError:
      pass

class Trainer(object):
  """ Trainer is responsible for setting up the players and game.
  """
  def __init__(self, args, rng=numpy.random.RandomState()):
    self._rng = rng # The Random Number Generator
    self._numOffense = args.numOffense # Number offensive players
    self._numDefense = args.numDefense # Number defensive players
    self._maxTrials = args.numTrials # Maximum number of trials to play
    self._maxFrames = args.numFrames # Maximum number of frames to play
    # =============== FIELD DIMENSIONS =============== #
    self.NUM_FRAMES_TO_HOLD = 2 # Hold ball this many frames to capture
    self.HOLD_FACTOR = 1.5 # Gain to calculate ball control
    self.PITCH_WIDTH = 68.0 # Width of the field
    self.PITCH_LENGTH = 105.0 # Length of field in long-direction
    self.UNTOUCHED_LENGTH = 100 # Trial will end if ball untouched for this long
    # allowedBallX, allowedBallY defines the usable area of the playfield
    self._allowedBallX = numpy.array([-0.1, 0.5 * self.PITCH_LENGTH])
    self._allowedBallY = numpy.array([-0.5 * self.PITCH_WIDTH, 0.5 * self.PITCH_WIDTH])
    # =============== COUNTERS =============== #
    self._numFrames = 0 # Number of frames seen in HFO trials
    self._frame = 0 # Current frame id
    self._lastTrialStart = -1 # Frame Id in which the last trial started
    self._lastFrameBallTouched = -1 # Frame Id in which ball was last touched
    # =============== TRIAL RESULTS =============== #
    self._numTrials = 0 # Total number of HFO trials
    self._numGoals = 0 # Trials in which the offense scored a goal
    self._numBallsCaptured = 0 # Trials in which defense captured the ball
    self._numBallsOOB = 0 # Trials in which ball went out of bounds
    self._numOutOfTime = 0 # Trials that ran out of time
    # =============== AGENT =============== #
    self._agent = not args.no_agent # Indicates if a learning agent is active
    self._agent_play_offense = args.play_offense # Agent's role
    self._agentTeam = '' # Name of the team the agent is playing for
    self._agentNumInt = -1 # Agent's internal team number
    self._agentNumExt = -1 # Agent's external team number
    # =============== MISC =============== #
    self._offenseTeam = '' # Name of the offensive team
    self._defenseTeam = '' # Name of the defensive team
    self._playerPositions = numpy.zeros((11,2,2)) # Positions of the players
    self._ballPosition = numpy.zeros(2) # Position of the ball
    self._ballHeld = numpy.zeros((11,2)) # Track player holding the ball
    self._teams = [] # Team indexes for offensive and defensive teams
    self._SP = {} # Sever Parameters. Recieved when connecting to the server.
    self._isPlaying = False # Is a game being played?
    self._agentPopen = None # Agent's process
    self.initMsgHandlers()

  def launch_agent(self):
    """Launch the learning agent using the start.sh script and return a
    DummyPopen for the process.
    """
    print '[Trainer] Launching Agent'
    if self._agent_play_offense:
      assert self._numOffense > 0
      self._agentTeam = self._offenseTeam
      self._agentNumInt = 1 if self._numOffense == 1 \
                          else self._rng.randint(1, self._numOffense)
      numTeammates = self._numOffense - 1
      numOpponents = self._numDefense
    else:
      assert self._numDefense > 0
      self._agentTeam = self._defenseTeam
      self._agentNumInt = 0 if self._numDefense == 1 \
                          else self._rng.randint(0, self._numDefense)
      numTeammates = self._numOffense
      numOpponents = self._numDefense - 1
    self._agentNumExt = self.convertToExtPlayer(self._agentTeam,
                                                self._agentNumInt)
    agentCmd = 'start_agent.sh -t %s -u %i --numTeammates %i --numOpponents %i'\
               %(self._agentTeam, self._agentNumExt, numTeammates, numOpponents)
    agentCmd = agentCmd.split(' ')
    # Ignore stderr because librcsc continually prints to it
    kwargs = {}#'stderr':open('/dev/null','w')}
    p = subprocess.Popen(agentCmd, **kwargs)
    p.wait()
    with open('/tmp/start%i' % p.pid,'r') as f:
      output = f.read()
    pid = int(re.findall('PID: (\d+)',output)[0])
    return DummyPopen(pid)

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

  def setTeams(self):
    """ Sets the offensive and defensive teams and player rosters. """
    self._offenseTeamInd = 0
    self._offenseTeam = self._teams[self._offenseTeamInd]
    self._defenseTeam = self._teams[1-self._offenseTeamInd]
    offensive_roster = self.getOffensiveRoster(self._offenseTeam)
    defensive_roster = self.getDefensiveRoster(self._defenseTeam)
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
    self._comm = ClientCommunicator(port=6001)
    self.send('(init (version 8.0))')
    self.checkMsg('(init ok)',retryCount=5)
    # self.send('(eye on)')
    self.send('(ear on)')

  def _hear(self, body):
    """ Handle a hear message. """
    timestep,playerInfo,msg = body
    if len(playerInfo) != 3:
      return
    _,team,player = playerInfo
    if team != self._agentTeam:
      return
    if int(player) != self._agentNumExt:
      return
    try:
      length = int(msg[0])
    except:
      return
    msg = msg[1:length+1]
    if msg == 'START':
      if self._isPlaying:
        print '[Trainer] Already playing, ignoring message'
      else:
        self.startGame()
    elif msg == 'DONE':
      raise DoneError
    else:
      print '[Trainer] Unhandled message from agent: %s' % msg

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
    self.registerMsgHandler(self._handleSP,'server_param')
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
      print >>sys.stderr,'[Trainer] Error with message'
      print >>sys.stderr,'  expected: %s' % expectedMsg
      print >>sys.stderr,'  received: %s' % msg
      print >>sys.stderr,len(expectedMsg),len(msg)
      raise ValueError

  def extractPoint(self, msg):
    """ Extract a point from the provided message. """
    return numpy.array(map(float,msg[:2]))

  def convertToExtPlayer(self, team, num):
    """ Returns the external player number for a given player. """
    assert team == self._offenseTeam or team == self._defenseTeam
    if team == self._offenseTeam:
      return self._offenseOrder[num]
    else:
      return self._defenseOrder[num]

  def convertFromExtPlayer(self, team, num):
    """ Maps external player number to internal player number. """
    if team == self._offenseTeam:
      return self._offenseOrder.index(num)
    else:
      return self._defenseOrder.index(num)

  def seeGlobal(self, body):
    """Send a look message to extract global information on ball and
    player positions.

    """
    self.send('(look)')
    self._frame = int(body[0])
    for obj in body[1:]:
      objType = obj[0]
      objData = obj[1:]
      if objType[0] == 'g':
        continue
      elif objType[0] == 'b':
        self._ballPosition = self.extractPoint(objData)
      elif objType[0] == 'p':
        teamName = objType[1]
        team = self.teamToInd(teamName)
        playerNum = self.convertFromExtPlayer(teamName,int(objType[2]))
        self._playerPositions[playerNum,:,team] = self.extractPoint(objData)

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
        print '[Trainer] Updating handler for %s' % (' '.join(args))
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
      print '[Trainer] Unhandled message:',msg[0:2]
    else:
      handler(msg[prefixLength:])

  def ignoreMsg(self,*args,**kwargs):
    """ Ignore a certain type of message. """
    self.registerMsgHandler(lambda x: None,*args,**kwargs)

  def _handleSP(self, body):
    """ Handler for the sever params message. """
    for param in body:
      try:
        val = int(param[1])
      except:
        try:
          val = float(param[1])
        except:
          val = param[1]
      self._SP[param[0]] = val

  def listenAndProcess(self):
    """ Gather messages and process them. """
    msg = self.recv()
    assert((msg[0] == '(') and (msg[-1] == ')')),'|%s|' % msg
    msg = self.parseMsg(msg)
    self.handleMsg(msg)

  def _readTeamNames(self,body):
    """ Read the names of each of the teams. """
    self._teams = []
    for _,_,team in body:
      self._teams.append(team)
    time.sleep(0.1)
    self.send('(team_names)')

  def waitOnTeam(self, first):
    """Wait on a given team. First indicates if this is the first team
    connected or the second.

    """
    self.send('(team_names)')
    partial = ['ok','team_names']
    self.registerMsgHandler(self._readTeamNames,*partial,quiet=True)
    while len(self._teams) < (1 if first else 2):
      self.listenAndProcess()
    #self.unregisterMsgHandler(*partial)
    self.ignoreMsg(*partial,quiet=True)

  def checkIfAllPlayersConnected(self):
    """ Returns true if all players are connected. """
    self.send('(look)')
    partial = ['ok','look']
    self._numPlayers = 0
    def f(x):
      self._numPlayers = len(x) - 4 # -4 for time, ball, goal_l, and goal_r
      self.send('(look)')
    self.registerMsgHandler(f,*partial)
    while self._numPlayers != 2 * 11:
      self.listenAndProcess()
    self.ignoreMsg(*partial,quiet=True)

  def startGame(self):
    """ Starts a game of HFO. """
    self.reset()
    self.registerMsgHandler(self.seeGlobal, 'see_global')
    self.registerMsgHandler(self.seeGlobal, 'ok', 'look', quiet=True)
    #self.registerMsgHandler(self.checkBall,'ok','check_ball')
    self.send('(look)')
    self._isPlaying = True

  def calcBallHolder(self):
    '''Calculates the ball holder, returns results in teamInd, playerInd. '''
    totalHeld = 0
    for team in self._teams:
      for i in range(11):
        pos = self._playerPositions[i,:,self.teamToInd(team)]
        distBound = self._SP['kickable_margin'] + self._SP['player_size'] \
                    + self._SP['ball_size']
        distBound *= self.HOLD_FACTOR
        if numpy.linalg.norm(self._ballPosition - pos) < distBound:
          self._ballHeld[i,self.teamToInd(team)] += 1
          totalHeld += 1
        else:
          self._ballHeld[i,self.teamToInd(team)] = 0
    # If multiple players are close to the ball, no-one is holding
    if totalHeld > 1:
      self._ballHeld[:,:] = 0
    inds = numpy.transpose((self._ballHeld >= self.NUM_FRAMES_TO_HOLD).nonzero())
    assert(len(inds) <= 1)
    if len(inds) == 1:
      return inds[0,1],inds[0,0]
    else:
      return None,None

  def isGoal(self):
    """ Returns true if a goal has been scored. """
    return (self._ballPosition[0] > self._allowedBallX[1]) \
      and (numpy.abs(self._ballPosition[1]) < self._SP['goal_width'])

  def isOOB(self):
    """ Returns true if the ball is out of bounds. """
    return self._ballPosition[0] < self._allowedBallX[0] \
      or self._ballPosition[0] > self._allowedBallX[1] \
      or self._ballPosition[1] < self._allowedBallY[0] \
      or self._ballPosition[1] > self._allowedBallY[1]

  def movePlayer(self, team, internal_num, pos, convertToExt=True):
    """ Move a player to a specified position.

    Args:
      team: the team name of the player
      interal_num: the player's internal number
      pos: position to move player to
      convertToExt: convert interal player num to external
    """
    num = self.convertToExtPlayer(team, internal_num) if convertToExt \
          else internal_num
    self.send('(move (player %s %i) %f %f)' % (team, num, pos[0], pos[1]))

  def moveBall(self, pos):
    """ Moves the ball to a specified x,y position. """
    self.send('(move (ball) %f %f 0.0 0.0 0.0)' % tuple(pos))

  def randomPointInBounds(self, xBounds=None, yBounds=None):
    """Returns a random point inside of the box defined by xBounds,
    yBounds. Where xBounds=[x_min, x_max] and yBounds=[y_min,
    y_max]. Defaults to the xy-bounds of the playable HFO area.

    """
    if xBounds is None:
      xBounds = self.allowedBallX
    if yBounds is None:
      yBounds = self.allowedBallY
    pos = numpy.zeros(2)
    bounds = [xBounds, yBounds]
    for i in range(2):
      pos[i] = self._rng.rand() * (bounds[i][1] - bounds[i][0]) + bounds[i][0]
    return pos

  def boundPoint(self, pos):
    """Ensures a point is within the minimum and maximum bounds of the
    HFO playing area.

    """
    pos[0] = min(max(pos[0], self._allowedBallX[0]), self._allowedBallX[1])
    pos[1] = min(max(pos[1], self._allowedBallY[0]), self._allowedBallY[1])
    return pos

  def reset(self):
    """ Resets the HFO domain by moving the ball and players. """
    self.resetBallPosition()
    self.resetPlayerPositions()
    self.send('(recover)')
    self.send('(change_mode play_on)')
    self.send('(say RESET)')

  def resetBallPosition(self):
    """Reset the position of the ball for a new HFO trial. """
    self._ballPosition = self.boundPoint(self.randomPointInBounds(
      .2*self._allowedBallX+.05*self.PITCH_LENGTH, .8*self._allowedBallY))
    self.moveBall(self._ballPosition)

  def getOffensiveResetPosition(self):
    """ Returns a random position for an offensive player. """
    offsets = [
      [-1,-1],
      [-1,1],
      [1,1],
      [1,-1],
      [0,2],
      [0,-2],
      [-2,-2],
      [-2,2],
      [2,2],
      [2,-2],
    ]
    offset = offsets[self._rng.randint(len(offsets))]
    offset_from_ball = 0.1 * self.PITCH_LENGTH * self._rng.rand(2) + \
                       0.1 * self.PITCH_LENGTH * numpy.array(offset)
    return self.boundPoint(self._ballPosition + offset_from_ball)
    # return self._ballPosition

  def getDefensiveResetPosition(self):
    """ Returns a random position for a defensive player. """
    return self.boundPoint(self.randomPointInBounds(
      [0.5 * 0.5 * self.PITCH_LENGTH, 0.75 * 0.5 * self.PITCH_LENGTH],
      0.8 * self._allowedBallY))

  def resetPlayerPositions(self):
    """Reset the positions of the players. This is called after a trial
    ends to setup for the next trial.

    """
    # Always Move the offensive goalie
    self.movePlayer(self._offenseTeam, 0, [-0.5 * self.PITCH_LENGTH, 0])
    # Move the rest of the offense
    for i in xrange(1, self._numOffense + 1):
      self.movePlayer(self._offenseTeam, i, self.getOffensiveResetPosition())
    # Move the defensive goalie
    if self._numDefense > 0:
      self.movePlayer(self._defenseTeam, 0, [0.5 * self.PITCH_LENGTH,0])
    # Move the rest of the defense
    for i in xrange(1, self._numDefense):
      self.movePlayer(self._defenseTeam, i, self.getDefensiveResetPosition())

  def removeNonHFOPlayers(self):
    """Removes players that aren't involved in HFO game.

    The players whose numbers are greater than numOffense/numDefense
    are sent to left-field.

    """
    for i in xrange(self._numOffense + 1, 11):
      # Don't remove the learning agent
      if not self._agent or i != self._agentNumInt or \
         self._agentTeam != self._offenseTeam:
        self.movePlayer(self._offenseTeam, i, [-0.25 * self.PITCH_LENGTH, 0])
    for i in xrange(self._numDefense, 11):
      # Don't remove the learning agent
      if not self._agent or i != self._agentNumInt or \
         self._agentTeam != self._defenseTeam:
        self.movePlayer(self._defenseTeam, i, [-0.25 * self.PITCH_LENGTH, 0])

  def step(self):
    """ Takes a simulated step. """
    # self.send('(check_ball)')
    self.removeNonHFOPlayers()
    team_holding_ball, player_holding_ball = self.calcBallHolder()
    if team_holding_ball is not None:
      self._lastFrameBallTouched = self._frame
    if self.trialOver(team_holding_ball):
      self.updateResults(team_holding_ball)
      self.reset()

  def updateResults(self, team_holding_ball):
    """ Updates the various members after a trial has ended. """
    if self.isGoal():
      self._numGoals += 1
      result = 'Goal'
    elif self.isOOB():
      self._numBallsOOB += 1
      result = 'Out of Bounds'
    elif team_holding_ball not in [None,self._offenseTeamInd]:
      self._numBallsCaptured += 1
      result = 'Defense Captured'
    elif self._frame - self._lastFrameBallTouched > self.UNTOUCHED_LENGTH:
      self._lastFrameBallTouched = self._frame
      self._numOutOfTime += 1
      result = 'Ball untouched for too long'
    else:
      print '[Trainer] Error: Unable to detect reason for End of Trial!'
      sys.exit(1)
    print '[Trainer] EndOfTrial: %2i /%2i %5i %s'%\
      (self._numGoals, self._numTrials, self._frame, result)
    self._numTrials += 1
    self._numFrames += self._frame - self._lastTrialStart
    self._lastTrialStart = self._frame
    if (self._maxTrials > 0) and (self._numTrials >= self._maxTrials):
      raise DoneError
    if (self._maxFrames > 0) and (self._numFrames >= self._maxFrames):
      raise DoneError

  def trialOver(self, team_holding_ball):
    """Returns true if the trial has ended for one of the following
    reasons: Goal scored, out of bounds, captured by defense, or
    untouched for too long.

    """
    # The trial is still being setup, it cannot be over.
    if self._frame - self._lastTrialStart < 5:
      return False
    return self.isGoal() \
      or self.isOOB() \
      or (team_holding_ball not in [None, self._offenseTeamInd]) \
      or (self._frame - self._lastFrameBallTouched > self.UNTOUCHED_LENGTH)

  def printStats(self):
    print 'Num frames in completed trials : %i' % (self._numFrames)
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
        print '[Trainer] Something necessary closed (%s), exiting' % name
        return False
    return True

  def run(self, necProcesses):
    """ Run the trainer.
    """
    try:
      if self._agent:
        self._agentPopen = self.launch_agent()
        necProcesses.append([self._agentPopen,'agent'])
      self.startGame()
      while self.checkLive(necProcesses):
        prevFrame = self._frame
        self.listenAndProcess()
        if self._frame != prevFrame:
          self.step()
    except TimeoutError:
      print '[Trainer] Haven\'t heard from the server for too long, Exiting'
    except (KeyboardInterrupt, DoneError):
      print '[Trainer] Exiting'
    finally:
      if self._agentPopen is not None:
        self._agentPopen.send_signal(SIGINT)
      try:
        self._comm.sendMsg('(bye)')
      except:
        pass
      self._comm.close()
      self.printStats()
