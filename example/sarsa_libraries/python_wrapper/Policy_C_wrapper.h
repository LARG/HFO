#ifndef __POLICY_C_WRAPPER_H__
#define __POLICY_C_WRAPPER_H__

#include "SarsaAgent.h"
#include "FuncApprox.h"
#include "CMAC.h"
#include<iostream>

extern "C" {
  void* SarsaAgent_new(int numFeatures, int numActions, double learningRate, double epsilon, double lambda, void *FA, char *loadWeightsFile, char *saveWeightsFile)
  {
    CMAC *fa = reinterpret_cast<CMAC *>(FA);
    SarsaAgent *sa=new SarsaAgent(numFeatures, numActions, learningRate, epsilon, lambda, fa, loadWeightsFile, saveWeightsFile);
    void *ptr = reinterpret_cast<void *>(sa);
    return ptr; 
  }
  void SarsaAgent_update(void *ptr, double state[], int action, double reward, double discountFactor)
  {
    SarsaAgent *p = reinterpret_cast<SarsaAgent *>(ptr);
    p->update(state,action,reward,discountFactor); 
  }
  int SarsaAgent_selectAction(void *ptr, double state[])
  {
    SarsaAgent *p = reinterpret_cast<SarsaAgent *>(ptr);
    int action=p->selectAction(state);
    return action;
  }
  void SarsaAgent_endEpisode(void *ptr) 
  {
    SarsaAgent *p = reinterpret_cast<SarsaAgent *>(ptr);
    p->endEpisode();
  }
}
#endif
