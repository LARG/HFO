#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <math.h>

using namespace std;
using namespace hfo;

#define PI 3.14159265

// This agent demonstrates the use of the DRIBBLE_TO action. Before
// running this program, first Start HFO server:
// $./bin/HFO --offense-agents 1

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
    int step = 0;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Dribble in a circle around center field
      float target_x = sin((step % 360) * PI/180);
      float target_y = cos((step % 360) * PI/180);
      hfo.act(DRIBBLE_TO, target_x, target_y);
      // Advance the environment and get the game status
      status = hfo.step();
      step += 2;
    }
    // Check what the outcome of the episode was
    cout << "Episode " << episode << " ended with status: "
         << StatusToString(status) << endl;;
  }
  hfo.act(QUIT);
};
