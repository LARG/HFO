#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <math.h>

using namespace std;
using namespace hfo;

// This agent demonstrates the use of the KICK_TO action. Before
// running this program, first Start HFO server: $./bin/HFO
// --offense-agents 1

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
      float x = feature_vec[0];
      float y = feature_vec[1];
      float dist_to_target = sqrt(x*x + y*y) * 3;
      // Perform the action and recieve the current game status
      bool able_to_kick = feature_vec[5] > 0;
      if (able_to_kick) {
        // Valid kick speed varies in the range [0, 3]
        if (dist_to_target < .1) {
          // Max power kick to goal
          hfo.act(KICK_TO, 1., 0., 3.0);
        } else {
          // Kick to center of hfo field
          hfo.act(KICK_TO, 0., 0., dist_to_target);
        }
      } else {
        hfo.act(INTERCEPT);
      }
      // Advance the environment and get the game status
      status = hfo.step();
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << endl;;
  }
  hfo.act(QUIT);
};
