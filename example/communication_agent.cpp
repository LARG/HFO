#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

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
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Get any incoming communication
      std::string msg = hfo.hear();
      // TODO: [Sanmit] Do something with incoming communication
      // Perform the action
      hfo.act(DASH, 0, 0);
      // TODO: [Sanmit] Do something with outgoing communication
      hfo.say("Message");
      // Advance the environment and get the game status
      status = hfo.step();
    }
  }
  hfo.act(QUIT);
};
