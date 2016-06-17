#include <iostream>
#include <vector>
#include <HFO.hpp>
#include <cstdlib>
#include <stdio.h>
#include <math.h>
#include <iostream>

using namespace std;
using namespace hfo;

// This agent is intended to be run as a part of the passing_agents.sh script

// Server Connection Options. See printouts from bin/HFO.
feature_set_t features = HIGH_LEVEL_FEATURE_SET;
string config_dir = "bin/teams/base/config/formations-dt";
int port = 6000;
string server_addr = "localhost";
string team_name = "base_left";
bool goalie = false;

#define PI 3.14159265
int main(int argc, char** argv) {
  if (argc > 1) {
    port = atoi(argv[1]);
  }
  // Create the HFO environment
  HFOEnvironment hfo;
  // Connect to the server and request feature set. See manual for
  // more information on feature sets.
  hfo.connectToServer(features, config_dir, port, server_addr,
                      team_name, goalie);
  int unum = hfo.getUnum();
  status_t status = IN_GAME;
  for (int episode = 0; status != SERVER_DOWN; episode++) {
    status = IN_GAME;
    int agent_on_ball = 7;
    while (status == IN_GAME) {
      // Get the vector of state features for the current state
      const vector<float>& feature_vec = hfo.getState();
      // Get any incoming communication
      std::string msg = hfo.hear();
      if (!msg.empty()) {
        cout << "Agent-" << unum << " heard: " << msg.c_str() << endl;
        if (msg == "Pass") {
          agent_on_ball = unum;
        }
      }
      float x_pos = feature_vec[0];
      float y_pos = feature_vec[1];
      float target_x = 0;
      float target_y = unum == 11 ? .3 : -.3;
      bool in_position = (pow(x_pos-target_x, 2) + pow(y_pos-target_y,2)) < .001;
      bool able_to_kick = feature_vec[5] > 0;
      if (agent_on_ball == unum && in_position && able_to_kick) {
        int teammate_unum = unum == 11 ? 7 : 11;
        float teammate_x_pos = 0;
        float teammate_y_pos = -target_y;
        hfo.act(KICK_TO, teammate_x_pos, teammate_y_pos, 2.0);
        hfo.say("Pass");
        cout << "Agent-" << unum << " said: Pass" << endl;
        agent_on_ball = teammate_unum;
      } else {
        float dist_to_ball = feature_vec[3];
        float dist_to_teammate = feature_vec[13];
        action_t action = unum == agent_on_ball ? DRIBBLE_TO : MOVE_TO;
        hfo.act(action, target_x, target_y);
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
