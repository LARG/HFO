import socket, struct, thread, time

class HFO_Actions:
  ''' An enum of the possible HFO actions

  Dash(power, relative_direction)
  Turn(direction)
  Tackle(direction)
  Kick(power, direction)
  QUIT

  '''
  DASH, TURN, TACKLE, KICK, QUIT = range(5)

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

  def connectToAgentServer(self, server_port=6008):
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
    data = self.socket.recv(struct.calcsize('f')*self.numFeatures)
    if not data:
      print '[Agent Client] ERROR Recieved bad data from Server. Perhaps server closed?'
      self.cleanup()
      exit(1)
    features = struct.unpack('f'*self.numFeatures, data)
    return features

  def act(self, action):
    ''' Send an action and recieve the game status.'''
    self.socket.send(struct.pack("iff", *action))
    # Get the current game status
    data = self.socket.recv(struct.calcsize("i"))
    status = struct.unpack("i", data)[0]
    return status

  def cleanup(self):
    ''' Send a quit and close the connection to the agent's server. '''
    self.socket.send(struct.pack("i", HFO_Actions.QUIT))
    self.socket.close()
