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
  std::cout<<"Usage: ./high_level_sarsa_agent [Options]"<<std::endl;
  std::cout<<"Options:"<<std::endl;
  std::cout<<"  --numAgents <int>        Number of SARSA agents"<<std::endl;
  std::cout<<"                           Default: 1"<<std::endl;
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
  std::cout<<"  --help                   Displays this help and exit"<<std::endl;
}

// Returns the reward for SARSA based on current state
double getReward(int status) {
  double reward;
  if (status==hfo::GOAL) reward = 1;
  else if (status==hfo::CAPTURED_BY_DEFENSE) reward = -1;
  else if (status==hfo::OUT_OF_BOUNDS) reward = -1;
  else reward = 0;
  return reward;
}

// Fill state with only the required features from state_vec
void purgeFeatures(double *state, const std::vector<float>& state_vec,
                   int numTMates, bool oppPres) {

  int stateIndex = 0;

  // If no opponents ignore features Distance to Opponent
  // and Distance from Teammate i to Opponent are absent
  int tmpIndex = 9 + 3 * numTMates;

  for(int i = 0; i < state_vec.size(); i++) {

    // Ignore first six features and teammate proximity to opponent(when opponent is absent)and opponent features
    if(i < 6||(!oppPres && ((i>9+numTMates && i<=9+2*numTMates)||i==9))||i>9+6*numTMates) continue;

    // Ignore Angle and Uniform Number of Teammates
    int temp =  i-tmpIndex;
    if(temp > 0 && (temp % 3 == 2 || temp % 3 == 0)) continue;
    if (i > 9+6*numTMates) continue;
    state[stateIndex] = state_vec[i];
    stateIndex++;
  }
  //std::cout<<stateIndex<<"yo";
}

// Convert int to hfo::Action
hfo::action_t toAction(int action, const std::vector<float>& state_vec) {
  hfo::action_t a;
  switch(action) {
    case 0: a = hfo::SHOOT;
            break;
    case 1: a = hfo::DRIBBLE;
            break;
    default:int size = state_vec.size();
            a = hfo::PASS;/*,
                 state_vec[(size - 1) - (action - 2) * 3],
                 0.0};*/
  }
  return a;
}

void offenseAgent(int port, int numTMates, int numEpi, double learnR,
                  int suffix, bool oppPres, double eps) {

  // Number of features
  int numF = oppPres ? (4 + 4 * numTMates) : (3 + 3 * numTMates);
  // Number of actions
  int numA = 2 + numTMates;

  double discFac = 1;

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
                  "_" + std::to_string(suffix);
  wtFile = &s[0u];
  double lambda = 0;
  CMAC *fa = new CMAC(numF, numA, range, min, res);
  SarsaAgent *sa = new SarsaAgent(numF, numA, learnR, eps, lambda, fa, wtFile, wtFile);

  hfo::HFOEnvironment hfo;
  hfo::status_t status;
  hfo::action_t a;
  double state[numF];
  int action = -1;
  double reward;
  hfo.connectToServer(hfo::HIGH_LEVEL_FEATURE_SET,"../../bin/teams/base/config/formations-dt",6000,"localhost","base_left",false,"");
  for (int episode=0; episode < numEpi; episode++) {
    int count = 0;
    status = hfo::IN_GAME;
    action = -1;
    while (status == hfo::IN_GAME) {
      const std::vector<float>& state_vec = hfo.getState();
      // If has ball
      if(state_vec[5] == 1) {
        if(action != -1) {
          reward = getReward(status);
          sa->update(state, action, reward, discFac);
        }

        // Fill up state array
        purgeFeatures(state, state_vec, numTMates, oppPres);

	// Get raw action
        action = sa->selectAction(state);

        // Get hfo::Action
        a = toAction(action, state_vec);

      } else {
            a = hfo::MOVE;
      }
      if (a== hfo::PASS) {
           hfo.act(a,state_vec[(9+6*numTMates) - (action-2)*3]);
           //std::cout<<(9+6*numTMates) - (action-2)*3;
        } else {
           hfo.act(a);
        }
      status = hfo.step();
    }
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

  int numAgents = 1;
  int numEpisodes = 10;
  int basePort = 6000;
  double learnR = 0.1;
  int suffix = 0;
  bool opponentPresent = true;
  double eps = 0.01;
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
    }else {
      printUsage();
      return 0;
    }
  }

  int numTeammates = numAgents - 1;

  std::thread agentThreads[numAgents];
  for (int agent = 0; agent < numAgents; agent++) {
    agentThreads[agent] = std::thread(offenseAgent, basePort + agent,
                                      numTeammates, numEpisodes, learnR,
                                      suffix, opponentPresent, eps);
    usleep(500000L);
  }
  for (int agent = 0; agent < numAgents; agent++) {
    agentThreads[agent].join();
  }
  return 0;
}
