#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>

using namespace std;
using namespace hfo;

// This agent demonstrates the use of the MOVE_TO action to visit the
// corners of the play field. Before running this program, first Start
// HFO server: $./bin/HFO --offense-agents 1

// Server Connection Options. See printouts from bin/HFO.
feature_set_t features = HIGH_LEVEL_FEATURE_SET;
string config_dir = "bin/teams/base/config/formations-dt";
int port = 6000;
string server_addr = "localhost";
string team_name = "base_left";
bool goalie = false;

int main(int argc, char** argv) {
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the server and request feature set. See manual for
  // more information on feature sets.
  hfo.connectToServer(features, config_dir, port, server_addr,
                           team_name, goalie);
  float target_x = .82;
  float target_y = .9;
  status_t status = IN_GAME;
  for (int episode = 0; status != SERVER_DOWN; episode++) {
    status = IN_GAME;
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
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << endl;;
  }
  hfo.act(QUIT);
};
