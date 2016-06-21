#ifndef __HFO_HPP__
#define __HFO_HPP__

#include <string>
#include <vector>
#include "common.hpp"

class Agent;
namespace rcsc { class BasicClient; }

namespace hfo {

class HFOEnvironment {
 public:
  HFOEnvironment();
  virtual ~HFOEnvironment();

  /**
   * Connect to the server on the specified port. The
   * following information is provided by the ./bin/HFO
   *
   * feature_set: High or low level state features
   * config_dir: Config directory. Typically HFO/bin/teams/base/config/
   * server_port: port to connect to server on
   * server_addr: address of server
   * team_name: Name of team to join.
   * play_goalie: is this player the goalie
   * record_dir: record agent's states/actions/rewards to this directory
   */
  void connectToServer(feature_set_t feature_set=HIGH_LEVEL_FEATURE_SET,
                       std::string config_dir="bin/teams/base/config/formations-dt",
                       int server_port=6000,
                       std::string server_addr="localhost",
                       std::string team_name="base_left",
                       bool play_goalie=false,
                       std::string record_dir="");

  // Get the current state of the domain. Returns a reference to feature_vec.
  const std::vector<float>& getState();

  // Specify action type to take followed by parameters for that action
  virtual void act(action_t action, ...);
  virtual void act(action_t action, const std::vector<float>& params);

  // Send/receive communication from other agents
  virtual void say(const std::string& message);
  virtual std::string hear();

  // Returns the uniform number of the player
  virtual int getUnum();

  // Get the current player holding the ball
  virtual Player playerOnBall();

  // Indicates the agent is done and the environment should
  // progress. Returns the game status after the step
  virtual status_t step();

 private:
  rcsc::BasicClient* client;
  Agent* agent;
  long current_cycle;
  bool ready_for_action;
};

} // namespace hfo
#endif
