#include "FuncApprox.h"

FunctionApproximator::FunctionApproximator(int numF, int numA){

  numFeatures = numF;
  numActions  = numA;

}

void FunctionApproximator::setState(double s[]){

  for(int i = 0; i < numFeatures; i++){
    state[i] = s[i];
  }
}

int FunctionApproximator::getNumFeatures(){ 
  return numFeatures; 
}
  
int FunctionApproximator::getNumActions(){
  return numActions;  
}

int FunctionApproximator::argMaxQ(){

  int bestAction = 0;
  double bestValue = computeQ(bestAction);

  int numTies = 0;
  double EPS = 1.0e-4;

  for(int a = 1; a < getNumActions(); a++){
    
    double q = computeQ(a);

    if(fabs(q - bestValue) < EPS){
      numTies++;

      if(drand48() < (1.0 / (numTies + 1))){
	bestAction = a;
	bestValue = q;
      }
    }
    else if(q > bestValue){
      bestAction = a;
      bestValue = q;
      numTies = 0;
    }
  }

  return bestAction;
}

double FunctionApproximator::bestQ(){

  int bestAction = 0;
  double bestValue = computeQ(bestAction);

  int numTies = 0;
  double EPS = 1.0e-4;

  for(int a = 1; a < getNumActions(); a++){
    
    double q = computeQ(a);

    if(fabs(q - bestValue) < EPS){
      numTies++;

      if(drand48() < (1.0 / (numTies + 1))){
	bestAction = a;
	bestValue = q;
      }
    }
    else if(q > bestValue){
      bestAction = a;
      bestValue = q;
      numTies = 0;
    }
  }

  return bestValue;
}

