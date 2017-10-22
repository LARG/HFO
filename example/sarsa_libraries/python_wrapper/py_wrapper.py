from ctypes import *
import numpy as np
import getpass
import sys, os

isPy3=False
if sys.version_info[0] == 3:
  isPy3=True

username=getpass.getuser()


libs = cdll.LoadLibrary(os.path.join(os.path.dirname(__file__),'C_wrappers.so'))

libs.CMAC_new.argtypes=[c_int,c_int,POINTER(c_double),POINTER(c_double),POINTER(c_double)]
libs.CMAC_new.restype=c_void_p

libs.SarsaAgent_new.argtypes=[c_int,c_int,c_double,c_double,c_double,c_void_p,c_char_p,c_char_p]
libs.SarsaAgent_new.restype=c_void_p

libs.SarsaAgent_update.argtypes=[c_void_p,POINTER(c_double),c_int,c_double,c_double]
libs.SarsaAgent_update.restype=None

libs.SarsaAgent_selectAction.argtypes=[c_void_p,POINTER(c_double)]
libs.SarsaAgent_selectAction.restype=c_int

libs.SarsaAgent_endEpisode.argtypes=[c_void_p]
libs.SarsaAgent_endEpisode.restype=None


class CMAC(object):

  def __init__(self,numF,numA,r,m,res):
    arr1 = (c_double * len(r))(*r)
    arr2 = (c_double * len(m))(*m)
    arr3 = (c_double * len(res))(*res)
    self.obj = libs.CMAC_new(c_int(numF),c_int(numA),arr1,arr2,arr3)
    #print(self.obj)

class SarsaAgent(object):

  def __init__(self,numFeatures, numActions, learningRate, epsilon, Lambda, FA, loadWeightsFile, saveWeightsFile):
    p1=c_int(numFeatures)
    p2=c_int(numActions)
    p3=c_double(learningRate)
    p4=c_double(epsilon)
    p5=c_double(Lambda)
    p6=c_void_p(FA.obj)
    if isPy3:
      #utf-8 encoding required for python3
      p7=c_char_p(loadWeightsFile.encode('utf-8'))
      p8=c_char_p(saveWeightsFile.encode('utf-8'))
    else:
    #non encoded will do for python2
      p7=c_char_p(loadWeightsFile)
      p8=c_char_p(saveWeightsFile)
    self.obj = libs.SarsaAgent_new(p1,p2,p3,p4,p5,p6,p7,p8)
    #print(format(self.obj,'02x'))

  def update(self,state,action,reward,discountFactor):
    s = (c_double * len(state))(*state)
    a = c_int(action)
    r = c_double(reward)
    df = c_double(discountFactor)
    libs.SarsaAgent_update(c_void_p(self.obj),s,a,r,df)
    #print(format(self.obj,'02x'))

  def selectAction(self,state):
    s = (c_double * len(state))(*state)
    action=libs.SarsaAgent_selectAction(c_void_p(self.obj),s)
    #print(action)
    #print(format(self.obj,'02x'))
    return int(action)

  def endEpisode(self):
    libs.SarsaAgent_endEpisode(c_void_p(self.obj))
    #print(format(self.obj,'02x'))
    
    
     


    
