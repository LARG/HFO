#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

// Server Connection Options. See printouts from bin/HFO.
feature_set_t features = LOW_LEVEL_FEATURE_SET;
string config_dir = "bin/teams/base/config/formations-dt";
int port = 6000;
string server_addr = "localhost";
string team_name = "base_left";
bool goalie = false;

float arg1, arg2;

// Returns a random low-level action
action_t get_random_low_lv_action() {
  action_t action_indx = (action_t) ((rand() % 4) + DASH);
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
  return action_indx;
}

int main(int argc, char** argv) {
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the server and request low-level feature set. See
  // manual for more information on feature sets.
  hfo.connectToServer(features, config_dir, port, server_addr,
                      team_name, goalie);
  status_t status = IN_GAME;
  for (int episode = 0; status != SERVER_DOWN; episode++) {
    status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Perform the action and recieve the current game status
      hfo.act(get_random_low_lv_action(), arg1, arg2);
      // Advance the environment and recieve current game status
      status = hfo.step();
    }
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << endl;;
  }
  hfo.act(QUIT);
};
