#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

// Returns a random high-level action
Action get_random_high_lv_action() {
  action_t action_indx = (action_t) ((rand() % 4) + 4);
  Action act = {action_indx, 0, 0};
  return act;
}

int main() {
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the agent's server on port 6000 and request low-level
  // feature set. See manual for more information on feature sets.
  hfo.connectToAgentServer(6000, HIGH_LEVEL_FEATURE_SET);
  // Play 5 episodes
  for (int episode=0; ; episode++) {
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Create a dash action
      Action a = get_random_high_lv_action();
      // Perform the dash and recieve the current game status
      status = hfo.act(a);
    }
  }
};
