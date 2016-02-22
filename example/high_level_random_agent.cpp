#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

// Server Connection Options. See printouts from bin/HFO.
feature_set_t features = HIGH_LEVEL_FEATURE_SET;
string config_dir = "bin/teams/base/config/formations-dt";
int unum = 11;
int port = 6001;
string server_addr = "localhost";
string team_name = "base_left";
bool goalie = false;

// Returns a random high-level action
action_t get_random_high_lv_action() {
  action_t action_indx = (action_t) ((rand() % 5) + MOVE);
  return action_indx;
}

int main(int argc, char** argv) {
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the server and request high-level feature set. See
  // manual for more information on feature sets.
  hfo.connectToServer(features, config_dir, unum, port, server_addr,
                           team_name, goalie);
  for (int episode=0; episode<10; episode++) {
    status_t status = IN_GAME;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Perform the action
      hfo.act(get_random_high_lv_action());
      // Advance the environment and get the game status
      status = hfo.step();
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << std::endl;
  }
  hfo.act(QUIT);
};
