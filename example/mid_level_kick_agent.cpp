#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <math.h>

using namespace std;
using namespace hfo;

// This agent demonstrates the use of the KICK_TO action. Before
// running this program, first Start HFO server: $./bin/HFO
// --offense-agents 1

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
  for (int episode=0; ; episode++) {
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      float x = feature_vec[0];
      float y = feature_vec[1];
      float dist_to_target = sqrt(x*x + y*y) * 3;
      // Perform the action and recieve the current game status
      bool able_to_kick = feature_vec[5] > 0;
      if (able_to_kick) {
        // Valid kick speed varies in the range [0, 3]
        if (dist_to_target < .1) {
          // Max power kick to goal
          hfo.act(KICK_TO, 1., 0., 3.0);
        } else {
          // Kick to center of hfo field
          hfo.act(KICK_TO, 0., 0., dist_to_target);
        }
      } else {
        hfo.act(INTERCEPT);
      }
      // Advance the environment and get the game status
      status = hfo.step();
    }
  }
  hfo.act(QUIT);
};
