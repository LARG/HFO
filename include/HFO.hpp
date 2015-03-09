#ifndef __HFO_HPP__
#define __HFO_HPP__

#include <vector>
#include "agent.h"

class HFOEnvironment {
 public:
  HFOEnvironment();
  ~HFOEnvironment();

  // Connect to the server that controls the agent on the specified port.
  void connectToAgentServer(int server_port=6008);

  // Get the current state of the domain. Returns a reference to feature_vec.
  const std::vector<float>& getState();

  // The actions available to the agent
  // enum action_t
  // {
  //   DASH,   // Dash(power, relative_direction)
  //   TURN,   // Turn(direction)
  //   TACKLE, // Tackle(direction)
  //   KICK    // Kick(power, direction)
  // };
  // Take an action and recieve the resulting game status
  hfo_status_t act(Action action);

 protected:
  int numFeatures; // The number of features in this domain
  int sockfd; // Socket file desriptor for connection to agent server
  std::vector<float> feature_vec; // Holds the features

  // Handshake with the agent server to ensure data is being correctly
  // passed. Also sets the number of features to expect.
  virtual void handshakeAgentServer();
};

#endif
