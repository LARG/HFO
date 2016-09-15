#ifndef CMAC_H
#define CMAC_H

#include <cmath>
#include "FuncApprox.h"
#include "tiles2.h"

#define RL_MEMORY_SIZE 1048576
#define RL_MAX_NONZERO_TRACES 100000
#define RL_MAX_NUM_TILINGS 6000

class CMAC: public FunctionApproximator{

 protected:

  int tiles[MAX_ACTIONS][RL_MAX_NUM_TILINGS];

  double minimumTrace;
  int nonzeroTraces[RL_MAX_NONZERO_TRACES];
  int numNonzeroTraces;
  int nonzeroTracesInverse[RL_MEMORY_SIZE];

  double ranges[MAX_STATE_VARS];
  double minValues[MAX_STATE_VARS];
  double resolutions[MAX_STATE_VARS];

  double weights[RL_MEMORY_SIZE];
  double traces [RL_MEMORY_SIZE];

  int numTilings;

  collision_table *colTab;

  void clearTrace(int f);
  void clearExistentTrace(int f, int loc);
  void setTrace(int f, double newTraceValue);
  void updateTrace(int f, double deltaTraceValue);
  void increaseMinTrace();
  
  void reset();

  void loadTiles();

  double getRange(int i); 
  double getMinValue(int i); 
  double getResolution(int i); 

 public:

  CMAC(int numF, int numA, double r[], double m[], double res[]);

  void setState(double s[]);

  void updateWeights(double delta, double alpha);

  void decayTraces(double decayRate);

  void read (char *fileName);
  void write(char *fileName);

  //Not implemented by CMAC
  int getNumWeights();
  void getWeights(double w[]);
  void setWeights(double w[]);

  double computeQ(int action);
  void clearTraces(int action);
  void updateTraces(int action);

};

#endif
