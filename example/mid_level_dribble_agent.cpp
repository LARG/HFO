#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <math.h>

using namespace std;
using namespace hfo;

// This agent demonstrates the use of the DRIBBLE_TO action. Before
// running this program, first Start HFO server: $./bin/HFO
// --offense-agents 1

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
  for (int episode=0; ; episode++) {
    status_t status = IN_GAME;
    int step = 0;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Dribble in a circle around center field
      float target_x = sin((step % 360) * PI/180);
      float target_y = cos((step % 360) * PI/180);
      status = hfo.act(DRIBBLE_TO, target_x, target_y);
      step += 2;
    }
  }
  hfo.act(QUIT);
};
