import socket, struct, thread, time

class HFO_Actions:
  ''' An enum of the possible HFO actions

  [Low-Level] Dash(power, relative_direction)
  [Low-Level] Turn(direction)
  [Low-Level] Tackle(direction)
  [Low-Level] Kick(power, direction)
  [High-Level] Move(): Reposition player according to strategy
  [High-Level] Shoot(): Shoot the ball
  [High-Level] Pass(): Pass to the most open teammate
  [High-Level] Dribble(): Offensive dribble
  QUIT

  '''
  DASH, TURN, TACKLE, KICK, MOVE, SHOOT, PASS, DRIBBLE, QUIT = range(9)

class HFO_Status:
  ''' Current status of the HFO game. '''
  IN_GAME, GOAL, CAPTURED_BY_DEFENSE, OUT_OF_BOUNDS, OUT_OF_TIME = range(5)


class HFOEnvironment(object):
  ''' The HFOEnvironment is designed to be the main point of contact
  between a learning agent and the Half-Field-Offense domain.

  '''
  def __init__(self):
    self.socket = None # Socket connection to server
    self.numFeatures = None # Given by the server in handshake
    self.features = None # The state features

  def connectToAgentServer(self, server_port=6000):
    '''Connect to the server that controls the agent on the specified port. '''
    self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print '[Agent Client] Connecting to Agent Server on port', server_port
    while True:
      try:
        self.socket.connect(('localhost', server_port))
      except:
        time.sleep(1)
        continue
      else:
        break
    print '[Agent Client] Connected'
    self.handshakeAgentServer()
    # Get the initial state
    state_data = self.socket.recv(struct.calcsize('f')*self.numFeatures)
    if not state_data:
      print '[Agent Client] ERROR Recieved bad data from Server. Perhaps server closed?'
      self.cleanup()
      exit(1)
    self.features = struct.unpack('f'*self.numFeatures, state_data)

  def handshakeAgentServer(self):
    '''Handshake with the agent's server. '''
    # Recieve float 123.2345
    data = self.socket.recv(struct.calcsize("f"))
    f = struct.unpack("f", data)[0]
    assert abs(f - 123.2345) < 1e-4, "Float handshake failed"
    # Send float 5432.321
    self.socket.send(struct.pack("f", 5432.321))
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

  def act(self, action):
    ''' Send an action and recieve the game status.'''
    self.socket.send(struct.pack("iff", *action))
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
    return status

  def cleanup(self):
    ''' Send a quit and close the connection to the agent's server. '''
    self.socket.send(struct.pack("i", HFO_Actions.QUIT))
    self.socket.close()
