import socket, struct, thread, time

class HFO_Features:
  ''' An enum of the possible HFO feature sets. For descriptions see
  https://github.com/mhauskn/HFO/blob/master/doc/manual.pdf
  '''
  LOW_LEVEL_FEATURE_SET, HIGH_LEVEL_FEATURE_SET = range(2)


class HFO_Actions:
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
  QUIT(): Quit the game

  '''
  DASH, TURN, TACKLE, KICK, KICK_TO, MOVE_TO, DRIBBLE_TO, INTERCEPT, \
    MOVE, SHOOT, PASS, DRIBBLE, CATCH, NOOP, QUIT = range(15)

class HFO_Status:
  ''' Current status of the HFO game. '''
  IN_GAME, GOAL, CAPTURED_BY_DEFENSE, OUT_OF_BOUNDS, OUT_OF_TIME = range(5)


class HFOEnvironment(object):
  ''' The HFOEnvironment is designed to be the main point of contact
  between a learning agent and the Half-Field-Offense domain.

  '''
  def __init__(self):
    self.socket = None           # Socket connection to server
    self.numFeatures = None      # Given by the server in handshake
    self.features = None         # The state features
    self.requested_action = None # Action to execute and parameters
    self.say_msg = None          # Outgoing message to say
    self.hear_msg = None         # Incoming heard message

  def NumParams(self, action_type):
    ''' Returns the number of required parameters for each action type. '''
    return {
      HFO_Actions.DASH : 2,
      HFO_Actions.TURN : 1,
      HFO_Actions.TACKLE : 1,
      HFO_Actions.KICK : 2,
      HFO_Actions.KICK_TO : 3,
      HFO_Actions.MOVE_TO : 2,
      HFO_Actions.DRIBBLE_TO : 2,
      HFO_Actions.INTERCEPT : 0,
      HFO_Actions.MOVE : 0,
      HFO_Actions.SHOOT : 0,
      HFO_Actions.PASS : 1,
      HFO_Actions.DRIBBLE : 0,
      HFO_Actions.CATCH : 0,
      HFO_Actions.NOOP : 0,
      HFO_Actions.QUIT : 0}.get(action_type, -1);

  def connectToAgentServer(self, server_port=6000,
                           feature_set=HFO_Features.HIGH_LEVEL_FEATURE_SET):
    '''Connect to the server that controls the agent on the specified port. '''
    self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print '[Agent Client] Connecting to Agent Server on port', server_port
    retry = 10
    while retry > 0:
      try:
        self.socket.connect(('localhost', server_port))
      except:
        time.sleep(1)
        retry -= 1
        continue
      else:
        break
    if retry <= 0:
      print '[Agent Client] ERROR Unable to communicate with server'
      exit(1)
    print '[Agent Client] Connected'
    self.handshakeAgentServer(feature_set)
    # Get the initial state
    state_data = self.socket.recv(struct.calcsize('f')*self.numFeatures)
    if not state_data:
      print '[Agent Client] ERROR Recieved bad data from Server. Perhaps server closed?'
      self.cleanup()
      exit(1)
    self.features = struct.unpack('f'*self.numFeatures, state_data)

  def handshakeAgentServer(self, feature_set):
    '''Handshake with the agent's server. '''
    # Recieve float 123.2345
    data = self.socket.recv(struct.calcsize("f"))
    f = struct.unpack("f", data)[0]
    assert abs(f - 123.2345) < 1e-4, "Float handshake failed"
    # Send float 5432.321
    self.socket.send(struct.pack("f", 5432.321))
    # Send the feature set request
    self.socket.send(struct.pack("i", feature_set))
    # Recieve the number of features
    data = self.socket.recv(struct.calcsize("i"))
    self.numFeatures = struct.unpack("i", data)[0]
    # Send what we recieved
    self.socket.send(struct.pack("i", self.numFeatures))
    # Get the current game status
    data = self.socket.recv(struct.calcsize("i"))
    status = struct.unpack("i", data)[0]
    assert status == HFO_Status.IN_GAME, "Status check failed"
    print '[Agent Client] Handshake complete'

  def getState(self):
    '''Get the current state of the world. Returns a list of floats with
    size numFeatures. '''
    return self.features

  def act(self, *args):
    ''' Send an action and recieve the game status.'''
    assert len(args) > 0, 'Not enough arguments provided to act'
    action_type = args[0]
    n_params = self.NumParams(action_type)
    assert n_params == len(args) - 1, 'Incorrect number of params to act: '\
      'Required %d provided %d'%(n_params, len(args)-1)
    self.requested_action = args

  def say(self, message):
    ''' Send a communication message to other agents. '''
    self.say_msg = message

  def hear(self):
    ''' Receive incoming communications from other players. '''
    return self.hear_msg

  def step(self):
    ''' Indicates the agent is done and the environment should
        progress. Returns the game status after the step'''
    # Send action and parameters
    self.socket.send(struct.pack('i'+'f'*(len(self.requested_action)-1),
                                 *self.requested_action))
    # TODO: [Sanmit] Send self.say_msg
    self.say_msg = ''
    # Get the current game status
    data = self.socket.recv(struct.calcsize("i"))
    status = struct.unpack("i", data)[0]
    # Get the next state features
    state_data = self.socket.recv(struct.calcsize('f')*self.numFeatures)
    if not state_data:
      print '[Agent Client] ERROR Recieved bad data from Server. Perhaps server closed?'
      self.cleanup()
      exit(1)
    self.features = struct.unpack('f'*self.numFeatures, state_data)
    self.hear_msg = ''
    # TODO: [Sanmit] Receive self.hear_msg
    return status

  def cleanup(self):
    ''' Send a quit and close the connection to the agent's server. '''
    self.socket.send(struct.pack("i", HFO_Actions.QUIT))
    self.socket.close()
