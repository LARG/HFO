#ifndef POLICY_AGENT
#define POLICY_AGENT

#include <cstring>
#include <fstream>
#include <iostream>
#include "FuncApprox.h"

#define MAX_STATE_VARS 100
#define MAX_ACTIONS 10

class PolicyAgent{

 private:

  int numFeatures;
  int numActions;

 protected:

  double learningRate;
  double epsilon;

  bool toLoadWeights;
  char loadWeightsFile[256];

  bool toSaveWeights;
  char saveWeightsFile[256];

  FunctionApproximator *FA;

  int getNumFeatures();
  int getNumActions();

 public:

  PolicyAgent(int numFeatures, int numActions, double learningRate, double epsilon, FunctionApproximator *FA, char *loadWeightsFile, char *saveWeightsFile);
  ~PolicyAgent();

  virtual int  argmaxQ(double state[]);
  virtual double computeQ(double state[], int action);

  virtual int selectAction(double state[]) = 0;
  
  virtual void update(double state[], int action, double reward, double discountFactor) = 0;
  virtual void endEpisode() = 0;
  virtual void reset() = 0;

  virtual void loadWeights(char *filename);
  virtual void saveWeights(char *filename);

};

#endif
