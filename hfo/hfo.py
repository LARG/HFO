from ctypes import *
import numpy as np
from numpy.ctypeslib import as_ctypes
import os

hfo_lib = cdll.LoadLibrary(os.path.join(os.path.dirname(__file__),
                                        'libhfo_c.so'))

''' Possible feature sets '''
LOW_LEVEL_FEATURE_SET, HIGH_LEVEL_FEATURE_SET = list(range(2))

''' An enum of the possible HFO actions
  [Low-Level] Dash(power, relative_direction)
  [Low-Level] Turn(direction)
  [Low-Level] Tackle(direction)
  [Low-Level] Kick(power, direction)
  [Mid-Level] Kick_To(target_x, target_y, speed)
  [Mid-Level] Move(target_x, target_y)
  [Mid-Level] Dribble(target_x, target_y)
  [Mid-Level] Intercept(): Intercept the ball
  [High-Level] Move(): Reposition player according to strategy
  [High-Level] Shoot(): Shoot the ball
  [High-Level] Pass(teammate_unum): Pass to teammate
  [High-Level] Dribble(): Offensive dribble
  [High-Level] Catch(): Catch the ball (Goalie Only)
  NOOP(): Do Nothing
  QUIT(): Quit the game '''
DASH, TURN, TACKLE, KICK, KICK_TO, MOVE_TO, DRIBBLE_TO, INTERCEPT, \
    MOVE, SHOOT, PASS, DRIBBLE, CATCH, NOOP, QUIT, REDUCE_ANGLE_TO_GOAL,MARK_PLAYER,DEFEND_GOAL,GO_TO_BALL = list(range(19))

''' Possible game status
  [IN_GAME] Game is currently active
  [GOAL] A goal has been scored by the offense
  [CAPTURED_BY_DEFENSE] The defense has captured the ball
  [OUT_OF_BOUNDS] Ball has gone out of bounds
  [OUT_OF_TIME] Trial has ended due to time limit
  [SERVER_DOWN] Server is not alive
'''
IN_GAME, GOAL, CAPTURED_BY_DEFENSE, OUT_OF_BOUNDS, OUT_OF_TIME, SERVER_DOWN = list(range(6))

''' Possible sides '''
RIGHT, NEUTRAL, LEFT = list(range(-1,2))

class Player(Structure): pass
Player._fields_ = [
    ('side', c_int),
    ('unum', c_int),
]

hfo_lib.HFO_new.argtypes = None
hfo_lib.HFO_new.restype = c_void_p
hfo_lib.HFO_del.argtypes = [c_void_p]
hfo_lib.HFO_del.restype = None
hfo_lib.connectToServer.argtypes = [c_void_p, c_int, c_char_p, c_int,
                                    c_char_p, c_char_p, c_bool, c_char_p]
hfo_lib.connectToServer.restype = None
hfo_lib.getStateSize.argtypes = [c_void_p]
hfo_lib.getStateSize.restype = c_int
hfo_lib.getState.argtypes = [c_void_p, c_void_p]
hfo_lib.getState.restype = None
hfo_lib.act.argtypes = [c_void_p, c_int, c_void_p]
hfo_lib.act.restype = None
hfo_lib.say.argtypes = [c_void_p, c_char_p]
hfo_lib.say.restype = None
hfo_lib.hear.argtypes = [c_void_p]
hfo_lib.hear.restype = c_char_p
hfo_lib.playerOnBall.argtypes = [c_void_p]
hfo_lib.playerOnBall.restype = Player
hfo_lib.step.argtypes = [c_void_p]
hfo_lib.step.restype = c_int
hfo_lib.numParams.argtypes = [c_int]
hfo_lib.numParams.restype = c_int
hfo_lib.actionToString.argtypes = [c_int]
hfo_lib.actionToString.restype = c_char_p
hfo_lib.statusToString.argtypes = [c_int]
hfo_lib.statusToString.restype = c_char_p
hfo_lib.getUnum.argtypes = [c_void_p]
hfo_lib.getUnum.restype = c_int

class HFOEnvironment(object):
  def __init__(self):
    self.obj = hfo_lib.HFO_new()

  def __del__(self):
    hfo_lib.HFO_del(self.obj)

  def connectToServer(self,
                      feature_set=LOW_LEVEL_FEATURE_SET,
                      config_dir='bin/teams/base/config/formations-dt',
                      server_port=6000,
                      server_addr='localhost',
                      team_name='base_left',
                      play_goalie=False,
                      record_dir=''):
    """
      Connect to the server on the specified port. The
      following information is provided by the ./bin/HFO

      feature_set: High or low level state features
      config_dir: Config directory. Typically HFO/bin/teams/base/config/
      server_port: port to connect to server on
      server_addr: address of server
      team_name: Name of team to join.
      play_goalie: is this player the goalie
      record_dir: record agent's states/actions/rewards to this directory
    """
    hfo_lib.connectToServer(self.obj, feature_set, config_dir.encode('utf-8'), server_port,server_addr.encode('utf-8'), team_name.encode('utf-8'), play_goalie, record_dir.encode('utf-8'))
  def getStateSize(self):
    """ Returns the number of state features """
    return hfo_lib.getStateSize(self.obj)

  def getState(self, state_data=None):
    """ Returns the current state features """
    if state_data is None:
      state_data = np.zeros(self.getStateSize(), dtype=np.float32)
    hfo_lib.getState(self.obj, as_ctypes(state_data))
    return state_data

  def act(self, *args):
    """ Performs an action in the environment """
    assert len(args) > 0, 'Not enough arguments provided to act'
    action_type = args[0]
    n_params = hfo_lib.numParams(action_type)
    assert n_params == len(args) - 1, 'Incorrect number of params to act: '\
      'Required %d, provided %d'%(n_params, len(args)-1)
    params = np.asarray(args[1:], dtype=np.float32)
    hfo_lib.act(self.obj, action_type, params.ctypes.data_as(POINTER(c_float)))

  def say(self, message):
    """ Transmit a message """
    hfo_lib.say(self.obj, message.encode('utf-8'))

  def hear(self):
    """ Returns the message heard from another player """
    return hfo_lib.hear(self.obj).decode('utf-8')

  def playerOnBall(self):
    """ Returns a player object who last touched the ball """
    return hfo_lib.playerOnBall(self.obj)

  def step(self):
    """ Advances the state of the environment """
    return hfo_lib.step(self.obj)

  def actionToString(self, action):
    """ Returns a string representation of an action """
    return hfo_lib.actionToString(action.decode('utf-8'))

  def statusToString(self, status):
    """ Returns a string representation of a game status """
    return hfo_lib.statusToString(status)

  def getUnum(self):
    """ Return the uniform number of the agent """
    return hfo_lib.getUnum(self.obj)
