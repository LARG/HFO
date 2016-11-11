#include "PolicyAgent.h"

PolicyAgent::PolicyAgent(int numFeatures, int numActions, double learningRate, double epsilon, FunctionApproximator *FA, char *loadWeightsFile, char *saveWeightsFile){

  this->numFeatures = numFeatures;
  this->numActions = numActions;

  this->learningRate = learningRate;
  this->epsilon = epsilon;
  this->FA = FA;

  toLoadWeights = strlen(loadWeightsFile) > 0;
  if(toLoadWeights){
    strcpy(this->loadWeightsFile, loadWeightsFile);
    loadWeights(loadWeightsFile);
  }

  toSaveWeights = strlen(saveWeightsFile) > 0;
  if(toSaveWeights){
    strcpy(this->saveWeightsFile, saveWeightsFile);
  }

}

PolicyAgent::~PolicyAgent(){
}

int PolicyAgent::getNumFeatures(){ 
  return numFeatures; 
}

int PolicyAgent::getNumActions(){ 
  return numActions;  
}

void PolicyAgent::loadWeights(char *fileName){
  std::cout << "Loading Weights from " << fileName << std::endl;
  FA->read(fileName);
}

void PolicyAgent::saveWeights(char *fileName){
  FA->write(fileName);
}

int  PolicyAgent::argmaxQ(double state[]){
  return ((int)(drand48() * getNumActions()) % getNumActions());
}

double PolicyAgent::computeQ(double state[], int action){
  return 0;
}

