#!/usr/bin/env python
# encoding: utf-8

import sys, numpy, time, os, subprocess, re
from optparse import Values
from signal import SIGINT
from Communicator import ClientCommunicator, TimeoutError

ADHOC_DIR = os.path.expanduser('~/research/adhoc2/robocup/adhoc-agent/')
#ADHOC_CMD = 'bin/start.sh -t %s -u %i --offenseAgents %s --defenseAgents %s'
ADHOC_CMD = 'bin/start.sh -t %s -u %i'

class DoneError(Exception):
  def __init__(self,msg='unknown'):
    self.msg = msg
  def __str__(self):
    return 'Done due to %s' % self.msg

class DummyPopen(object):
  def __init__(self,pid):
    self.pid = pid

  def poll(self):
    try:
      os.kill(self.pid,0)
      return None
    except OSError:
      return 0

  def send_signal(self,sig):
    try:
      os.kill(self.pid,sig)
    except OSError:
      pass

class Trainer(object):
  # numDefense is excluding the goalie
  def __init__(self,seed=None, options=Values()):
    self._options = options
    self._numOffense = self._options.numOffense + 1
    self._numDefense = self._options.numDefense + 1
    self._teams = []
    self._lastTrialStart = -1
    self._numFrames = 0
    self._lastFrameBallTouched = -1
    self._maxTrials = self._options.numTrials
    self._maxFrames = self._options.numFrames

    self._rng = numpy.random.RandomState(seed)
  
    self._playerPositions = numpy.zeros((11,2,2))
    self._ballPosition = numpy.zeros(2)
    self._ballHeld = numpy.zeros((11,2))
    self._frame = 0
    self._SP = {}
    self.NUM_FRAMES_TO_HOLD = 2
    self.HOLD_FACTOR = 1.5
    self.PITCH_WIDTH = 68.0
    self.PITCH_LENGTH = 105.0
    self.UNTOUCHED_LENGTH = 100
    self._allowedBallX = numpy.array([-0.1,0.5 * self.PITCH_LENGTH])
    self._allowedBallY = numpy.array([-0.5 * self.PITCH_WIDTH,0.5 * self.PITCH_WIDTH])

    self._numTrials = 0
    self._numGoals = 0
    self._numBallsCaptured = 0
    self._numBallsOOB = 0

    self._adhocTeam = ''
    self._adhocNumInt = -1
    self._adhocNumExt = -1
    self._isPlaying = False

    self._adhocPopen = None

    self.initMsgHandlers()

  def launchAdhoc(self):
    # start ad hoc agent
    os.chdir(ADHOC_DIR)
    self._adhocTeam = self._offenseTeam if self._options.adhocOffense else self._defenseTeam
    if self._options.adhocOffense:
      self._adhocNumInt = self._rng.randint(1,self._numOffense)
    else:
      self._adhocNumInt = self._rng.randint(1,self._numDefense)
    self._adhocNumExt = self.convertToExtPlayer(self._adhocTeam,self._adhocNumInt)
    #adhocCmd = ADHOC_CMD % (teamName,playerNum,' '.join(map(str,self._offenseOrder[:self._numOffense])),' '.join(map(str,self._defenseOrder[:self._numDefense])))
    adhocCmd = ADHOC_CMD % (self._adhocTeam,self._adhocNumExt)
    adhocCmd = adhocCmd.split(' ')
    if self._options.learnActions:
      adhocCmd += ['--learn-actions',str(self._options.numLearnActions)]
    print 'AdhocCmd', adhocCmd
    p = subprocess.Popen(adhocCmd)
    p.wait()
    with open('/tmp/start%i' % p.pid,'r') as f:
      output = f.read()
    pid = int(re.findall('PID: (\d+)',output)[0])
    return DummyPopen(pid)

  def getPlayers(self,name):
    if name == 'Borregos':
      offense = [2,4,6,5,3,7,9,10,8,11]
      defense = [9,10,8,11,7,4,6,2,3,5]
    elif name == 'WrightEagle':
      offense = [11,4,7,3,6,10,8,9,2,5]
      defense = [5,2,8,9,10,6,3,11,4,7]
    else:
      offense = [11,7,8,9,10,6,3,2,4,5]
      defense = [2,3,4,5,6,7,8,11,9,10]
    return offense, defense


  def setTeams(self):
    #print 'SETTING TEAMS:',self._teams
    self._offenseTeamInd = 0
    self._offenseTeam = self._teams[self._offenseTeamInd]
    self._defenseTeam = self._teams[1-self._offenseTeamInd]
    o,_ = self.getPlayers(self._offenseTeam)
    _,d = self.getPlayers(self._defenseTeam)
    self._offenseOrder = [1] + o # 1 for goalie
    self._defenseOrder = [1] + d # 1 for goalie

  def teamToInd(self,team):
    return self._teams.index(team)

  def parseMsg(self,msg):
    assert(msg[0] == '(')
    res,ind = self.__parseMsg(msg,1)
    assert(ind == len(msg)),msg
    return res

  def __parseMsg(self,msg,ind):
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
    self._comm = ClientCommunicator(port=6001)
    self.send('(init (version 8.0))')
    self.checkMsg('(init ok)',retryCount=5)
    #self.send('(eye on)')
    self.send('(ear on)')

  def _hear(self,body):
    timestep,playerInfo,msg = body
    if len(playerInfo) != 3:
      return
    _,team,player = playerInfo
    if team != self._adhocTeam:
      return
    if int(player) != self._adhocNumExt:
      return
    try:
      length = int(msg[0])
    except:
      return
    msg = msg[1:length+1]
    if msg == 'START':
      if self._isPlaying:
        print 'Already playing, ignoring message'
      else:
        self.startGame()
    elif msg == 'RESWI':
      self.reset('reset learning action with ball',False,True,True)
    elif msg == 'RESWO':
      self.reset('reset learning action withOUT ball',False,True,False)
    elif msg == 'DONE':
      raise DoneError
    else:
      print 'Unhandled message from ad hoc agent: %s' % msg

  def initMsgHandlers(self):
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

  def recv(self,retryCount=None):
    return self._comm.recvMsg(retryCount=retryCount).strip()

  def send(self,msg):
    self._comm.sendMsg(msg)

  def checkMsg(self,expectedMsg,retryCount=None):
    msg = self.recv(retryCount)
    if msg != expectedMsg:
      print >>sys.stderr,'Error with message'
      print >>sys.stderr,'  expected: %s' % expectedMsg
      print >>sys.stderr,'  received: %s' % msg
      print >>sys.stderr,len(expectedMsg),len(msg)
      raise ValueError

  def extractPoint(self,msg):
    return numpy.array(map(float,msg[:2]))

  def convertToExtPlayer(self,team,num):
    if team == self._offenseTeam:
      return self._offenseOrder[num]
    else:
      return self._defenseOrder[num]

  def convertFromExtPlayer(self,team,num):
    if team == self._offenseTeam:
      return self._offenseOrder.index(num)
    else:
      return self._defenseOrder.index(num)

  def seeGlobal(self,body):
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
    '''register a message handler
    handler will be called on a message that matches *args'''
    args = list(args)
    i,_,_ = self._findHandlerInd(args)
    if i < 0:
      self._msgHandlers.append([args,handler])
    else:
      if ('quiet' not in kwargs) or (not kwargs['quiet']):
        print 'Updating handler for %s' % (' '.join(args))
      self._msgHandlers[i] = [args,handler]

  def unregisterMsgHandler(self,*args):
    i,_,_ = self._findHandlerInd(args)
    assert(i >= 0)
    del self._msgHandlers[i]

  def _findHandlerInd(self,msg):
    msg = list(msg)
    for i,(partial,handler) in enumerate(self._msgHandlers):
      recPartial = msg[:len(partial)]
      if recPartial == partial:
        return i,len(partial),handler
    return -1,None,None

  def handleMsg(self,msg):
    i,prefixLength,handler = self._findHandlerInd(msg)
    if i < 0:
      print 'Unhandled message:',msg[0:2]
    else:
      handler(msg[prefixLength:])

  def ignoreMsg(self,*args,**kwargs):
    self.registerMsgHandler(lambda x: None,*args,**kwargs)

  def _handleSP(self,body):
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
    msg = self.recv()
    assert((msg[0] == '(') and (msg[-1] == ')')),'|%s|' % msg
    msg = self.parseMsg(msg)
    self.handleMsg(msg)

  def _readTeamNames(self,body):
    self._teams = []
    for _,_,team in body:
      self._teams.append(team)
    time.sleep(0.1)
    self.send('(team_names)')

  def waitOnTeam(self,first):
    self.send('(team_names)')
    partial = ['ok','team_names']
    self.registerMsgHandler(self._readTeamNames,*partial,quiet=True)
    while len(self._teams) < (1 if first else 2):
      self.listenAndProcess()
    #self.unregisterMsgHandler(*partial)
    self.ignoreMsg(*partial,quiet=True)

  def checkIfAllPlayersConnected(self):
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
    self.reset('Start',False)
    self.registerMsgHandler(self.seeGlobal,'see_global')
    self.registerMsgHandler(self.seeGlobal,'ok','look',quiet=True)
    #self.registerMsgHandler(self.checkBall,'ok','check_ball')
    self.send('(look)')
    self._isPlaying = True

  def calcBallHolder(self):
    '''calculates the ball holder, returns results in teamInd,playerInd'''
    totalHeld = 0
    for team in self._teams:
      for i in range(11):
        pos = self._playerPositions[i,:,self.teamToInd(team)]
        distBound = self._SP['kickable_margin'] + self._SP['player_size'] + self._SP['ball_size']
        distBound *= self.HOLD_FACTOR
        if numpy.linalg.norm(self._ballPosition - pos) < distBound:
          self._ballHeld[i,self.teamToInd(team)] += 1
          totalHeld += 1
        else:
          self._ballHeld[i,self.teamToInd(team)] = 0
    if totalHeld > 1:
      self._ballHeld[:,:] = 0
    inds = numpy.transpose((self._ballHeld >= self.NUM_FRAMES_TO_HOLD).nonzero())
    assert(len(inds) <= 1)
    if len(inds) == 1:
      return inds[0,1],inds[0,0]
    else:
      return None,None

  def isGoal(self):
    return (self._ballPosition[0] > self._allowedBallX[1]) and (numpy.abs(self._ballPosition[1]) < self._SP['goal_width'])

  def isOOB(self):
    # check ball x
    #return self._ballPosition[0] < self._allowedBallX[0]
    if (self._ballPosition[0] < self._allowedBallX[0]) or (self._ballPosition[0] > self._allowedBallX[1]):
      return True
    # check ball y
    if (self._ballPosition[1] < self._allowedBallY[0]) or (self._ballPosition[1] > self._allowedBallY[1]):
      return True
    return False

  def movePlayer(self,team,num,pos,convertToExt=True):
    if convertToExt:
      num = self.convertToExtPlayer(team,num)
    self.send('(move (player %s %i) %f %f)' % (team,num,pos[0],pos[1]))

  def moveBall(self,pos):
    self.send('(move (ball) %f %f 0.0 0.0 0.0)' % tuple(pos))

  def randomPosInBounds(self,xBounds,yBounds):
    pos = numpy.zeros(2)
    bounds = [xBounds,yBounds]
    for i in range(2):
      pos[i] = self._rng.rand() * (bounds[i][1] - bounds[i][0]) + bounds[i][0]
    return pos

  def boundPoint(self,pos):
    # x
    if pos[0] < self._allowedBallX[0]:
      pos[0] = self._allowedBallX[0]
    elif pos[0] > self._allowedBallX[1]:
      pos[0] = self._allowedBallX[1]
    # y
    if pos[1] < self._allowedBallY[0]:
      pos[1] = self._allowedBallY[0]
    elif pos[1] > self._allowedBallY[1]:
      pos[1] = self._allowedBallY[1]
    return pos

  def reset(self,msg,inc=True,learnActionReset=False,adhocAgentHasBall=False):
    if inc:
      self._numTrials += 1
      self._numFrames += self._frame - self._lastTrialStart
    if not learnActionReset:
      print '%2i /%2i %5i %s' % (self._numGoals,self._numTrials,self._frame,msg)
    if (self._maxTrials > 0) and (self._numTrials >= self._maxTrials):
      raise DoneError
    if (self._maxFrames > 0) and (self._numFrames >= self._maxFrames):
      raise DoneError

    if learnActionReset:
      self.resetPositionsActionLearning(adhocAgentHasBall)
    else:
      self.resetPositions()
    self.send('(recover)')
    self.send('(change_mode play_on)')
    self.send('(say RESET)')
    self._lastTrialStart = self._frame

  def resetPositionsActionLearning(self,adhocAgentHasBall):
    for i in range(1,self._numDefense):
      if adhocAgentHasBall and (not self._options.adhocOffense) and (i == self._adhocNumInt): continue
      pos = self.boundPoint(self.randomPosInBounds(self._allowedBallX,self._allowedBallY))
      self.movePlayer(self._defenseTeam,i,pos)
    for i in range(1,self._numOffense):
      if adhocAgentHasBall and self._options.adhocOffense and (i == self._adhocNumInt): continue
      pos = self.boundPoint(self.randomPosInBounds(self._allowedBallX,self._allowedBallY))
      self.movePlayer(self._offenseTeam,i,pos)
    self._ballPosition = self.boundPoint(self.randomPosInBounds(self._allowedBallX,self._allowedBallY))
    self.moveBall(self._ballPosition)
    if adhocAgentHasBall:
      # start the agent with the ball in the kickable margin
      r = self._rng.rand() * self._SP['kickable_margin']
      a = self._rng.rand() * 2 * numpy.pi
      offset = r * numpy.array([numpy.cos(a),numpy.sin(a)])
      self.movePlayer(self._adhocTeam,self._adhocNumExt,self._ballPosition + offset,convertToExt=False)

  def resetPositions(self):
    self._ballPosition = self.boundPoint(self.randomPosInBounds(0.20 * self._allowedBallX + 0.05 * self.PITCH_LENGTH,0.8 * self._allowedBallY))
    self.moveBall(self._ballPosition)
    # set up offense
    self.movePlayer(self._offenseTeam,0,[-0.5 * self.PITCH_LENGTH,0])
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
    for i,o in zip(range(1,self._numOffense),offsets):
      offset = 0.1 * self.PITCH_LENGTH * self._rng.rand(2) + 0.1 * self.PITCH_LENGTH * numpy.array(o)
      pos = self.boundPoint(self._ballPosition + offset)
      self.movePlayer(self._offenseTeam,i,pos)
    # set up defense
    self.movePlayer(self._defenseTeam,0,[0.5 * self.PITCH_LENGTH,0])
    for i in range(1,self._numDefense):
      pos = self.boundPoint(self.randomPosInBounds([0.5 * 0.5 * self.PITCH_LENGTH,0.75 * 0.5 * self.PITCH_LENGTH],0.8 * self._allowedBallY))
      self.movePlayer(self._defenseTeam,i,pos)

  def nullifyOtherPlayers(self):
    # offense
    for i in range(self._numOffense,11):
      self.movePlayer(self._offenseTeam,i,[-0.25 * self.PITCH_LENGTH,0])
    # defense
    for i in range(self._numDefense,11):
      self.movePlayer(self._defenseTeam,i,[-0.25 * self.PITCH_LENGTH,0])

  def step(self):
    #print 'step',self._frame
    #self.send('(check_ball)')
    self.nullifyOtherPlayers()
    heldTeam,heldPlayer = self.calcBallHolder()
    if heldTeam is not None:
      self._lastFrameBallTouched = self._frame

    # don't reset too fast, stuff is still happening
    if self._frame - self._lastTrialStart < 5:
      return
    if not self._options.learnActions:
      self.doResetIfNeeded(heldTeam)

  def doResetIfNeeded(self,heldTeam):
    if self.isGoal():
      self._numGoals += 1
      self.reset('Goal')
      return
    if self.isOOB():
      self._numBallsOOB += 1
      self.reset('Out of bounds')
      return
    if heldTeam not in [None,self._offenseTeamInd]:
      self._numBallsCaptured += 1
      self.reset('Defense captured')
      return
    if self._frame - self._lastFrameBallTouched > self.UNTOUCHED_LENGTH:
      self._lastFrameBallTouched = self._frame
      self.reset('Untouched for too long',False)
      return

  def printStats(self):
    print ''
    print 'Num frames in completed trials : %i' % (self._numFrames)
    print 'Trials             : %i' % self._numTrials
    print 'Goals              : %i' % self._numGoals
    print 'Defense Captured   : %i' % self._numBallsCaptured
    print 'Balls Out of Bounds: %i' % self._numBallsOOB

  def checkLive(self,necProcesses):
    for p,name in necProcesses:
      if p.poll() is not None:
        print 'Something necessary closed (%s), exiting' % name
        return False
    return True

  def run(self,necProcesses):
    try:
      if self._options.useAdhoc:
        self._adhocPopen = self.launchAdhoc() # will take care of starting game once adhoc is ready
        necProcesses.append([self._adhocPopen,'adhoc'])
      else:
        print 'starting game'
        self.startGame()
      while self.checkLive(necProcesses):
        prevFrame = self._frame
        self.listenAndProcess()
        if self._frame != prevFrame:
          self.step()
    except TimeoutError:
      print 'Haven\'t heard from the server for too long, Exiting'
    except (KeyboardInterrupt, DoneError):
      print 'Exiting'
    finally:
      if self._adhocPopen is not None:
        self._adhocPopen.send_signal(SIGINT)
      try:
        self._comm.sendMsg('(bye)')
      except:
        pass
      self._comm.close()

      self.printStats()

if __name__ == '__main__':
  #seed = int(time.time())
  assert(len(sys.argv) == 1)
  #team1 = sys.argv[1]
  #team2 = sys.argv[2]
  seed = int(sys.argv[1])
  print 'Random Seed:',seed
  t = Trainer(seed=seed)
  t.run()
