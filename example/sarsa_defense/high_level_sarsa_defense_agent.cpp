#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <thread>
#include "SarsaAgent.h"
#include "CMAC.h"
#include <unistd.h>

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents numAgents

void printUsage() {
  std::cout<<"Usage:123 ./high_level_sarsa_agent [Options]"<<std::endl;
  std::cout<<"Options:"<<std::endl;
  std::cout<<"  --numAgents <int>        Number of SARSA agents"<<std::endl;
  std::cout<<"                           Default: 0"<<std::endl;
  std::cout<<"  --numEpisodes <int>      Number of episodes to run"<<std::endl;
  std::cout<<"                           Default: 10"<<std::endl;
  std::cout<<"  --basePort <int>         SARSA agent base port"<<std::endl;
  std::cout<<"                           Default: 6000"<<std::endl;
  std::cout<<"  --learnRate <float>      Learning rate of SARSA agents"<<std::endl;
  std::cout<<"                           Range: [0.0, 1.0]"<<std::endl;
  std::cout<<"                           Default: 0.1"<<std::endl;
  std::cout<<"  --suffix <int>           Suffix for weights files"<<std::endl;
  std::cout<<"                           Default: 0"<<std::endl;
  std::cout<<"  --noOpponent             Sets opponent present flag to false"<<std::endl;
  std::cout<<"  --step                   Sets the persistent step size"<<std::endl;
  std::cout<<"  --eps                    Sets the exploration rate"<<std::endl;
  std::cout<<"  --numOpponents           Sets the number of opponents"<<std::endl;
  std::cout<<"  --weightId               Sets the given Id for weight File"<<std::endl;
  std::cout<<"  --help                   Displays this help and exit"<<std::endl;
}

// Returns the reward for SARSA based on current state
double getReward(hfo::status_t status) {
  double reward;
  if (status==hfo::GOAL) reward = -1;
  else if (status==hfo::CAPTURED_BY_DEFENSE) reward = 1;
  else if (status==hfo::OUT_OF_BOUNDS) reward = 1;
  else reward = 0;
  return reward;
}

// Fill state with only the required features from state_vec
void purgeFeatures(double *state, const std::vector<float>& state_vec,
                   int numTMates, int numOpponents, bool oppPres) {

  int stateIndex = 0;

  // Features[0 - 9] - {5,8}=8
  // Features[9+T+1 - 9+2T]: teammates dists to closest opps=T
  // Features [9+3T+1 - 9+6T]: x, y, unum of teammates ignoring %3 -> unum of team mates=2T
  // Features  [9+6T+1 - 9+6T+3*O]: x, y, unum of opponents ignoring %3 -> unum of opponents=20
  // Ignored: Feature [ 9+6T+3O+1, 9+6T+3O+2]: last_action_status,stamina->2 

  // If no opponents ignore features Distance to Opponent
  // and Distance from Teammate i to Opponent are absent
  int tmpIndex = oppPres ? (9 + 3 * numTMates) : (9 + 2 * numTMates);

  for(int i = 0; i < state_vec.size()-2; i++) {

    // Ignore first six featues
    if(i == 5 || i==8) continue;
    if(i>9 && i<= 9+numTMates) continue; // Ignore Goal Opening angles, as invalid
    if(i<= 9+3*numTMates && i > 9+2*numTMates) continue; // Ignore Pass Opening angles, as invalid
    // Ignore Uniform Number of Teammates and opponents
    int temp =  i-tmpIndex;
    if(temp > 0 && (temp % 3 == 0) )continue;
    //if (i > 9+6*numTMates) continue;
    state[stateIndex] = state_vec[i];
    stateIndex++;
  }
}

// Convert int to hfo::Action
hfo::action_t toAction(int action, const std::vector<float>& state_vec) {
hfo::action_t a;
  switch (action) {
        case 0: a = hfo::MOVE; break;
        case 1: a = hfo::REDUCE_ANGLE_TO_GOAL; break;
        case 2: a = hfo::GO_TO_BALL; break;
        case 3: a = hfo::NOOP; break;
        case 4: a = hfo::DEFEND_GOAL; break;
        default : a = hfo::MARK_PLAYER; break;
  }
    return a;
}

void offenseAgent(int port, int numTMates, int numOpponents, int numEpi, double learnR,
                  int suffix, bool oppPres, double eps, int step, std::string weightid) { 

  // Number of features
  int numF = oppPres ? (8 + 3 * numTMates + 2*numOpponents) : (3 + 3 * numTMates);
  // Number of actions
  int numA = 5 + numOpponents; //DEF_GOAL+MOVE+GTB+NOOP+RATG+MP(unum)

  // Other SARSA parameters
  eps = 0.01;
  double discFac = 1;
  double lambda=0.9375;
  // Tile coding parameter
  double resolution = 0.1;

  double range[numF];
  double min[numF];
  double res[numF];
  for(int i = 0; i < numF; i++) {
      min[i] = -1;
      range[i] = 2;
      res[i] = resolution;
  }

  // Weights file
  char *wtFile;
  std::string s = "weights_" + std::to_string(port) +
                  "_" + std::to_string(numTMates + 1) +
                  "_" + std::to_string(suffix) + 
                  "_" + std::to_string(step) +
                   "_" + weightid;
  wtFile = &s[0u];

  CMAC *fa = new CMAC(numF, numA, range, min, res);
  SarsaAgent *sa = new SarsaAgent(numF, numA, learnR, eps, lambda, fa, wtFile, wtFile);

  hfo::HFOEnvironment hfo;
  hfo::status_t status;
  hfo::action_t a;
  double state[numF];
  int action = -1;
  double reward;
  int no_of_offense = numTMates + 1;
  hfo.connectToServer(hfo::HIGH_LEVEL_FEATURE_SET,"../../bin/teams/base/config/formations-dt",port,"localhost","base_right",false,"");


  for (int episode=0; episode < numEpi; episode++) {
    int count = 0;
    status = hfo::IN_GAME;
    action = -1;
    int count_steps = 0;
    double unum = -1;
    const std::vector<float>& state_vec = hfo.getState();
    int num_steps_per_epi = 0;
    while (status == hfo::IN_GAME) {
          num_steps_per_epi++;
      //std::cout << "::::::"<< hfo::ActionToString(a) <<" "<<count_steps <<std::endl;  
      if (count_steps != step && action >=0 && (a != hfo :: MARK_PLAYER ||  unum>0)) {
          count_steps ++;
          if (a == hfo::MARK_PLAYER) {
              hfo.act(a,unum);
              //std::cout << "MARKING" << unum <<"\n";
          } else {
              hfo.act(a);
          }
          status = hfo.step();
          continue;
          
      } else {
          count_steps = 0;
      }
      
      if(action != -1) {
        reward = getReward(status);
        sa->update(state, action, reward, discFac);
      }

      // Fill up state array
      purgeFeatures(state, state_vec, numTMates, numOpponents, oppPres);

          // Get raw action
      action = sa->selectAction(state);

      // Get hfo::Action
      a = toAction(action, state_vec);
      if (a== hfo::MARK_PLAYER) {
           unum = state_vec[(state_vec.size()-1 -2 - (action-5)*3)];
           hfo.act(a,unum);
        } else {
           hfo.act(a);
        }
        std::string s = std::to_string(action);
        for (int state_vec_fc=0; state_vec_fc < state_vec.size(); state_vec_fc++) {
            s+=std::to_string(state_vec[state_vec_fc]) + ",";
        }
      s+="UNUM" +std::to_string(unum) +"\n";;
      status = hfo.step();
     // std::cout <<s;
    }
    //std :: cout <<":::::::::::::" << num_steps_per_epi<< " "<<step << " "<<"\n";
    // End of episode
    if(action != -1) {
      reward = getReward(status);
      sa->update(state, action, reward, discFac);
      sa->endEpisode();
    }
  }
  
  delete sa;
  delete fa;
}

int main(int argc, char **argv) {

  int numAgents = 0;
  int numEpisodes = 10;
  int basePort = 6000;
  double learnR = 0.1;
  int suffix = 0;
  bool opponentPresent = true;
  int numOpponents = 0;
  double eps = 0.01;
  int step = 10;
  std::string weightid;
  for (int i = 0; i<argc; i++) {
      std::string param = std::string(argv[i]);
      std::cout<<param<<"\n";
  }
  for(int i = 1; i < argc; i++) {
    std::string param = std::string(argv[i]);
    if(param == "--numAgents") {
      numAgents = atoi(argv[++i]);
    }else if(param == "--numEpisodes") {
      numEpisodes = atoi(argv[++i]);  
    }else if(param == "--basePort") {
      basePort = atoi(argv[++i]);
    }else if(param == "--learnRate") {
      learnR = atof(argv[++i]);
      if(learnR < 0 || learnR > 1) {
        printUsage();
        return 0;
      }
    }else if(param == "--suffix") {
      suffix = atoi(argv[++i]);
    }else if(param == "--noOpponent") {
      opponentPresent = false;
    }else if(param=="--eps"){
        eps=atoi(argv[++i]);
    }else if(param=="--numOpponents"){
        numOpponents=atoi(argv[++i]);
    }else if(param=="--step"){
        step=atoi(argv[++i]);
    }else if(param=="--weightId"){
        weightid=std::string(argv[++i]);
    }else {
      printUsage();
      return 0;
    }
  }
  int numTeammates = numAgents; //using goalie npc
  std::thread agentThreads[numAgents];
  for (int agent = 0; agent < numAgents; agent++) {
    agentThreads[agent] = std::thread(offenseAgent, basePort + agent,
                                      numTeammates, numOpponents, numEpisodes, learnR,
                                      suffix, opponentPresent, eps, step, weightid);
    usleep(500000L);
  }
  for (int agent = 0; agent < numAgents; agent++) {
    agentThreads[agent].join();
  }
  return 0;
}


