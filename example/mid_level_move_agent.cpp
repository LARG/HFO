#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// This agent demonstrates the use of the MOVE_TO action to visit the
// corners of the play field. Before running this program, first Start
// HFO server: $./bin/HFO --offense-agents 1
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
  float target_x = 1.0;
  float target_y = 1.0;
  for (int episode=0; ; episode++) {
    status_t status = IN_GAME;
    if (episode % 2 != 0) {
      target_x *= -1;
    } else {
      target_y *= -1;
    }
    std::cout << "target (x,y) = " << target_x << ", " << target_y << std::endl;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Perform the action
      hfo.act(MOVE_TO, target_x, target_y);
      // Advance the environment and get the game status
      status = hfo.step();
    }
  }
  hfo.act(QUIT);
};
