#include "SarsaAgent.h"

//add lambda as parameter to sarsaagent
SarsaAgent::SarsaAgent(int numFeatures, int numActions, double learningRate, double epsilon, double lambda, FunctionApproximator *FA, char *loadWeightsFile, char *saveWeightsFile):PolicyAgent(numFeatures, numActions, learningRate, epsilon, FA, loadWeightsFile, saveWeightsFile){
  this->lambda = lambda;
  episodeNumber = 0;
  lastAction = -1;
  //have memory for lambda
}

void SarsaAgent::update(double state[], int action, double reward, double discountFactor){

  if(lastAction == -1){

    for(int i = 0; i < getNumFeatures(); i++){
      lastState[i] = state[i];
    }
    lastAction = action;
    lastReward = reward;
  }
  else{

    FA->setState(lastState);

    double oldQ = FA->computeQ(lastAction);
    FA->updateTraces(lastAction);

    double delta = lastReward - oldQ;

    FA->setState(state);

    //Sarsa update
    double newQ = FA->computeQ(action);
    delta += discountFactor * newQ;

    FA->updateWeights(delta, learningRate);
    //Assume gamma, lambda are 0.
    FA->decayTraces(discountFactor*lambda);//replace 0 with gamma*lambda

    for(int i = 0; i < getNumFeatures(); i++){
      lastState[i] = state[i];
    }
    lastAction = action;
    lastReward = reward;
  }
}

void SarsaAgent::endEpisode(){

  episodeNumber++;
  //This will not happen usually, but is a safety.
  if(lastAction == -1){
    return;
  }
  else{

    FA->setState(lastState);
    double oldQ = FA->computeQ(lastAction);
    FA->updateTraces(lastAction);
    double delta = lastReward - oldQ;

    FA->updateWeights(delta, learningRate);
    //Assume lambda is 0. this comment looks wrong.
    FA->decayTraces(0);//remains 0
  }

  if(toSaveWeights && (episodeNumber + 1) % 5 == 0){
    saveWeights(saveWeightsFile);
    std::cout << "Saving weights to " << saveWeightsFile << std::endl;
  }

  lastAction = -1;

}

void SarsaAgent::reset(){
  
  lastAction = -1;
}

int SarsaAgent::selectAction(double state[]){

  int action;

  if(drand48() < epsilon){
    action = (int)(drand48() * getNumActions()) % getNumActions();
  }
  else{
    action = argmaxQ(state);
  }
  
  return action;
}

int SarsaAgent::argmaxQ(double state[]){
  
  double Q[getNumActions()];

  FA->setState(state);

  for(int i = 0; i < getNumActions(); i++){
    Q[i] = FA->computeQ(i);
  }
  
  int bestAction = 0;
  double bestValue = Q[bestAction];
  int numTies = 0;

  double EPS=1.0e-4;

  for (int a = 1; a < getNumActions(); a++){

    double value = Q[a];
    if(fabs(value - bestValue) < EPS){
      numTies++;
      
      if(drand48() < (1.0 / (numTies + 1))){
	bestValue = value;
	bestAction = a;
      }
    }
    else if (value > bestValue){
      bestValue = value;
      bestAction = a;
      numTies = 0;
    }
  }
  
  return bestAction;
}

//Be careful. This resets FA->state.
double SarsaAgent::computeQ(double state[], int action){

  FA->setState(state);
  double QValue = FA->computeQ(action);

  return QValue;
}

