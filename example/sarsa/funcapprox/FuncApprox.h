#ifndef FUNC_APPROX
#define FUNC_APPROX

#include <stdlib.h>
#include <math.h>

#define MAX_STATE_VARS 100
#define MAX_ACTIONS 10

class FunctionApproximator{

 protected:

  int numFeatures, numActions;
  double state[MAX_STATE_VARS];

  int getNumFeatures();
  int getNumActions();
  
 public:

  FunctionApproximator(int numF, int numA);
  virtual ~FunctionApproximator(){}
  

  virtual void setState(double s[]);

  virtual double computeQ(int action) = 0;
  virtual int argMaxQ();
  virtual double bestQ();
  virtual void updateWeights(double delta, double alpha) = 0;

  virtual void clearTraces(int action) = 0;
  virtual void decayTraces(double decayRate) = 0;
  virtual void updateTraces(int action) = 0;

  virtual void read (char *fileName) = 0;
  virtual void write(char *fileName) = 0;

  virtual int getNumWeights() = 0;
  virtual void getWeights(double w[]) = 0;
  virtual void setWeights(double w[]) = 0;

  virtual void reset() = 0;

};

#endif

