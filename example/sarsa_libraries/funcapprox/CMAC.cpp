#include "CMAC.h"

#define TILINGS_PER_GROUP 32

CMAC::CMAC(int numF, int numA, double r[], double m[], double res[]):FunctionApproximator(numF,numA){

  for(int i = 0; i < numF; i++){
    ranges[i] = r[i];
    minValues[i] = m[i];
    resolutions[i] = res[i];
  }

  minimumTrace = 0.01;

  numNonzeroTraces = 0;
  for(int i = 0; i < RL_MEMORY_SIZE; i++){
    weights[i] = 0;
    traces[i] = 0;
  }

  srand((unsigned int)0);
  int tmp[2];
  float tmpf[2];
  colTab = new collision_table( RL_MEMORY_SIZE, 1 );
    
  GetTiles(tmp, 1, 1, tmpf, 0);// A dummy call to set the hashing table    
}

double CMAC::getRange(int i){
  return ranges[i]; 
}

double CMAC::getMinValue(int i){
  return minValues[i]; 
}

double CMAC::getResolution(int i){ 
  return resolutions[i];
}

void CMAC::setState(double s[]){  
  FunctionApproximator::setState(s);
  loadTiles();
}

void CMAC::updateWeights(double delta, double alpha){

  double tmp = delta * alpha / numTilings;
  
  for(int i = 0; i < numNonzeroTraces; i++){

    int f = nonzeroTraces[i];
    if(f > RL_MEMORY_SIZE || f < 0){
      std::cerr << "f is too big or too small!!" << f << "\n";
    }

    weights[f] += tmp * traces[f];
  }
}

// Decays all the (nonzero) traces by decay_rate, removing those below minimum_trace 
void CMAC::decayTraces(double decayRate){

  int f;
  for(int loc = numNonzeroTraces - 1; loc >= 0; loc--){
    
    f = nonzeroTraces[loc];
    if(f > RL_MEMORY_SIZE || f < 0){
      std::cerr << "DecayTraces: f out of range " << f << "\n";
    }

    traces[f] *= decayRate;
    if(traces[f] < minimumTrace){
      clearExistentTrace(f, loc);
    }

  }
}

// Clear any trace for feature f      
void CMAC::clearTrace(int f){

  if(f > RL_MEMORY_SIZE || f < 0){
    std::cerr << "ClearTrace: f out of range " << f << "\n";
  }

  if(traces[f] != 0){
    clearExistentTrace(f, nonzeroTracesInverse[f]);
  }
}

// Clear the trace for feature f at location loc in the list of nonzero traces 
void CMAC::clearExistentTrace(int f, int loc){

  if(f > RL_MEMORY_SIZE || f < 0){
    std::cerr << "ClearExistentTrace: f out of range " << f << "\n";
  }

  traces[f] = 0.0;
  numNonzeroTraces--;
  nonzeroTraces[loc] = nonzeroTraces[numNonzeroTraces];
  nonzeroTracesInverse[nonzeroTraces[loc]] = loc;
}

// Set the trace for feature f to the given value, which must be positive   
void CMAC::setTrace(int f, double newTraceValue){

  if(f > RL_MEMORY_SIZE || f < 0){
    std::cerr << "SetTraces: f out of range " << f << "\n";
  }

  if(traces[f] >= minimumTrace){
    traces[f] = newTraceValue;// trace already exists              
  }
  else{

    while(numNonzeroTraces >= RL_MAX_NONZERO_TRACES){
      increaseMinTrace();// ensure room for new trace
    }

    traces[f] = newTraceValue;
    nonzeroTraces[numNonzeroTraces] = f;
    nonzeroTracesInverse[f] = numNonzeroTraces;
    numNonzeroTraces++;
  }
}

// Set the trace for feature f to the given value, which must be positive   
void CMAC::updateTrace(int f, double deltaTraceValue){

  setTrace(f, traces[f] + deltaTraceValue);
}

// Try to make room for more traces by incrementing minimum_trace by 10%,
// culling any traces that fall below the new minimum                      
void CMAC::increaseMinTrace(){

  minimumTrace *= 1.1;
  std::cerr << "Changing minimum_trace to " << minimumTrace << std::endl;

  for (int loc = numNonzeroTraces - 1; loc >= 0; loc--){ // necessary to loop downwards    
    int f = nonzeroTraces[loc];
    if(traces[f] < minimumTrace){
      clearExistentTrace(f, loc);
    }

  }
}

void CMAC::read(char *fileName){

  std::fstream file;
  file.open(fileName, std::ios::in | std::ios::binary);
  file.read((char *) weights, RL_MEMORY_SIZE * sizeof(double));
  unsigned long pos = file.tellg();
  file.close();

  colTab->restore(fileName, pos);
}

void CMAC::write(char *fileName){

  std::fstream file;
  file.open(fileName, std::ios::out | std::ios::binary);
  file.write((char *) weights, RL_MEMORY_SIZE * sizeof(double));
  unsigned long pos = file.tellp();
  file.close();

  colTab->save(fileName, pos);
}

void CMAC::reset(){
  
  for (int i = 0; i < RL_MEMORY_SIZE; i++){
    weights[i] = 0;
    traces[i] = 0;
  }
}

void CMAC::loadTiles(){

  int tilingsPerGroup = TILINGS_PER_GROUP;  /* num tilings per tiling group */
  numTilings = 0;
  
  /* These are the 'tiling groups'  --  play here with representations */
  /* One tiling for each state variable */
  for(int v = 0; v < getNumFeatures(); v++){

    for(int a = 0; a < getNumActions(); a++){
      GetTiles1(&(tiles[a][numTilings]), tilingsPerGroup, colTab, state[v] / getResolution(v), a , v);
    }  

    numTilings += tilingsPerGroup;

  }

  if(numTilings > RL_MAX_NUM_TILINGS){
    std::cerr << "TOO MANY TILINGS! " << numTilings << "\n";
  }
}

double CMAC::computeQ(int action){

  double q = 0;
  for(int j = 0; j < numTilings; j++){
    q += weights[tiles[action][j]];
  }

  return q;
}

void CMAC::clearTraces(int action){

  for(int j = 0; j < numTilings; j++){
    clearTrace(tiles[action][j]);
  }
}

void CMAC::updateTraces(int action){

  for(int j = 0; j < numTilings; j++)//replace/set traces F[a]
    setTrace(tiles[action][j], 1.0);
}

//Not implemented by CMAC
int CMAC::getNumWeights(){
  return 0;
}

//Not implemented by CMAC
void CMAC::getWeights(double w[]){
}

//Not implemented by CMAC
void CMAC::setWeights(double w[]){
}


