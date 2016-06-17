#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// Before running this program, first Start HFO server:
// $> ./bin/HFO --offense-agents 1

// Server Connection Options. See printouts from bin/HFO.
feature_set_t features = LOW_LEVEL_FEATURE_SET;
string config_dir = "bin/teams/base/config/formations-dt";
int port = 6000;
string server_addr = "localhost";
string team_name = "base_left";
bool goalie = false;

int main() {
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
      const std::vector<float>& feature_vec = hfo.getState();
      // Perform the dash
      hfo.act(DASH, 20.0, 0.0);
      // Advance the environment and recieve current game status
      status = hfo.step();
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << endl;
  }
  hfo.act(QUIT);
};
