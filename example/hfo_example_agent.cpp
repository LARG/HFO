#include <iostream>
#include <vector>
#include <HFO.hpp>

using namespace std;

// First Start the server by calling start.py in bin

int main() {
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect the agent's server which should be listening if
  // ./bin/start.py was called.
  hfo.connectToAgentServer();
  // Continue until finished
  while (true) {
    // Grab the vector of state features for the current state
    const std::vector<float>& feature_vec = hfo.getState();
    // Create a dash action
    Action a = {DASH, 100., 0.};
    // Perform the dash and recieve the reward
    float reward = hfo.act(a);
  }
};
