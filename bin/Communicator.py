#!/usr/bin/env python

'''
File: Communicator.py
Author: Samuel Barrett
Description: some classes for socket communication
Created:  2010-11-07
Modified: 2010-11-07
'''

import socket, sys, time
import cPickle as pickle

defaultPort = 5557

class TimeoutError(Exception):
  def __init__(self, *args):
    self.value = args
  def __str__(self):
    return repr(self.value)

class Communicator(object):
  def __init__(self,host='localhost',port=defaultPort,sock=None):
    self._sock = sock
    self._storedMsg = ''
    self._addr = (host,port)

    self.initialize()

  def initialize(self):
    if self._sock is None:
      raise ValueError

  def close(self):
    if self._sock is not None:
      try:
        self._sock.shutdown(socket.SHUT_RDWR)
        self._sock.close()
      except:
        pass
      finally:
        self._sock = None

  def sendMsg(self,msg):
    #print 'sending',msg
    self._sock.sendto(msg + '\0',self._addr)

  def recvMsg(self,event=None,retryCount=None):
    msg = self._storedMsg
    while ('\0' not in msg):
      if (event is not None) and event.isSet():
        return None
      newMsg = ''
      try:
        newMsg,self._addr = self._sock.recvfrom(8192)
        msg += newMsg
      except socket.error:
        #time.sleep(0.1)
        pass
      if len(newMsg) == 0:
        if (retryCount is None) or (retryCount <= 0):
          raise TimeoutError
        else:
          retryCount -= 1
          print '[Trainer] waiting for message, retry =', retryCount
          time.sleep(0.3)
          #raise ValueError('Error while receiving message')
    (msg,sep,rest) = msg.partition('\0')
    self._storedMsg = rest
    #print 'received',msg
    return msg

  def send(self,obj):
    self.sendMsg(pickle.dumps(obj))

  def recv(self,event=None):
    msg = self.recvMsg(event)
    if msg is None:
      return None
    return self.convertMsg(msg)

  def convertMsg(self,msg):
    return pickle.loads(msg)

class ClientCommunicator(Communicator):
  def initialize(self):
    try:
      self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      self._sock.settimeout(5)
    except:
      print >>sys.stderr,'Error creating socket'
      raise
