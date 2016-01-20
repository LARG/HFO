#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <stdio.h>
#include <math.h>
#include <iostream>
using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

#define PI 3.14159265
int main(int argc, char** argv) {
  int port = 6000;
  if (argc > 1) {
    port = atoi(argv[1]);
  }
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the agent's server on port 6000 and request low-level
  // feature set. See manual for more information on feature sets.
  hfo.connectToAgentServer(port, HIGH_LEVEL_FEATURE_SET);
  // Play 5 episodes
  for (int episode=0; ; episode++) {
    int step = 0;
    vector<int> game_status;
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Get any incoming communication
      std::string msg = hfo.hear();
      // Do something with incoming communication
      cout << "HEARD: " << msg.c_str() << endl;
      float target_x = sin((step % 360) * PI/180);
      float target_y = cos((step % 360) * PI/180);
      hfo.act(DRIBBLE_TO, target_x, target_y);

      // Do something with outgoing communication
      hfo.say("Message");
      // Advance the environment and get the game status
      game_status = hfo.step();
      status = (status_t)game_status[0];
      step+=2;
    }
  }
  hfo.act(QUIT);
};
