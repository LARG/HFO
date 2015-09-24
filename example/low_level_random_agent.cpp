#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

// Returns a random low-level action
Action get_random_low_lv_action() {
  action_t action_indx = (action_t) (rand() % 4);
  float arg1, arg2;
  switch (action_indx) {
    case DASH:
      arg1 = (rand() / float(RAND_MAX)) * 200 - 100; // power: [-100, 100]
      arg2 = (rand() / float(RAND_MAX)) * 360 - 180; // direction: [-180, 180]
      break;
    case TURN:
      arg1 = (rand() / float(RAND_MAX)) * 360 - 180; // direction: [-180, 180]
      arg2 = 0;
      break;
    case TACKLE:
      arg1 = (rand() / float(RAND_MAX)) * 360 - 180; // direction: [-180, 180]
      arg2 = 0;
      break;
    case KICK:
      arg1 = (rand() / float(RAND_MAX)) * 100;       // power: [0, 100]
      arg2 = (rand() / float(RAND_MAX)) * 360 - 180; // direction: [-180, 180]
      break;
    default:
      cout << "Invalid Action Index: " << action_indx;
      break;
  }
  Action act = {action_indx, arg1, arg2};
  return act;
}

int main() {
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the agent's server on port 6000 and request low-level
  // feature set. See manual for more information on feature sets.
  hfo.connectToAgentServer(6000, LOW_LEVEL_FEATURE_SET);
  // Play 5 episodes
  for (int episode=0; ; episode++) {
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Create a dash action
      Action a = get_random_low_lv_action();
      // Perform the dash and recieve the current game status
      status = hfo.act(a);
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: ";
    switch (status) {
      case GOAL:
        cout << "goal" << endl;
        break;
      case CAPTURED_BY_DEFENSE:
        cout << "captured by defense" << endl;
        break;
      case OUT_OF_BOUNDS:
        cout << "out of bounds" << endl;
        break;
      case OUT_OF_TIME:
        cout << "out of time" << endl;
        break;
      default:
        cout << "Unknown status " << status << endl;
        exit(1);
    }
  }
};
