import socket, struct, thread, time

class HFOEnvironment(object):
  '''The HFOEnvironment is designed to be the single point of contact
  between a learning agent and the Half-Field-Offense domain.

  '''

  def __init__(self):
    self.socket = None # Socket connection to server
    self.numFeatures = None # Given by the server in handshake
    self.trainerThreadID = None # Thread of the trainer process
    self.actions = ['DASH', 'TURN', 'TACKLE', 'KICK']

  def startDomain(self, args=[]):
    '''Covenience method to start the HFO domain by calling the
    /bin/start.py script and providing it kwargs. Call this method
    before connectToAgentServer.

    args: a list of argument strings passed to the start script.
    (e.g. ['--offense','3']). See ./bin/start.py -h for all args.
    '''
    # This method calls the trainer in bin directory
    def runTrainer():
      from bin import start
      start.main(start.parseArgs(args))
    self.trainerThreadID = thread.start_new_thread(runTrainer,())
    time.sleep(2)

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
    print '[Agent Client] Connected', server_port
    self.handshakeAgentServer()

  def handshakeAgentServer(self):
    '''Handshake with the agent's server. Returns the number of state
    features in the domain. '''
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

  def act(self, action_number):
    ''' Send an action and recieve the resulting reward from the environment.'''
    self.socket.send(struct.pack("i", action_number))
    return 0

  def cleanup(self):
    ''' Close the connection to the agent's server. '''
    self.socket.close()
    if self.trainerThreadID is not None:
      thread.interrupt_main()

if __name__ == '__main__':
  hfo = HFOEnvironment()
  trainer_args = '--offense 1 --defense 0 --headless'.split(' ')
  hfo.startDomain(trainer_args)
  hfo.connectToAgentServer()
  while True:
    features = hfo.getState()
    reward = hfo.act(0)
  hfo.cleanup()
