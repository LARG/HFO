#ifndef __HFO_HPP__
#define __HFO_HPP__

#include <string>
#include <vector>

namespace hfo {

// For descriptions of the different feature sets see
// https://github.com/mhauskn/HFO/blob/master/doc/manual.pdf
enum feature_set_t
{
  LOW_LEVEL_FEATURE_SET,
  HIGH_LEVEL_FEATURE_SET
};

// The actions available to the agent
enum action_t
{
  DASH,       // [Low-Level] Dash(power [0,100], direction [-180,180])
  TURN,       // [Low-Level] Turn(direction [-180,180])
  TACKLE,     // [Low-Level] Tackle(direction [-180,180])
  KICK,       // [Low-Level] Kick(power [0,100], direction [-180,180])
  KICK_TO,    // [Mid-Level] Kick_To(target_x [-1,1], target_y [-1,1], speed [0,3])
  MOVE_TO,    // [Mid-Level] Move(target_x [-1,1], target_y [-1,1])
  DRIBBLE_TO, // [Mid-Level] Dribble(target_x [-1,1], target_y [-1,1])
  INTERCEPT,  // [Mid-Level] Intercept(): Intercept the ball
  MOVE,       // [High-Level] Move(): Reposition player according to strategy
  SHOOT,      // [High-Level] Shoot(): Shoot the ball
  PASS,       // [High-Level] Pass(teammate_unum [0,11]): Pass to the most open teammate
  DRIBBLE,    // [High-Level] Dribble(): Offensive dribble
  CATCH,      // [High-Level] Catch(): Catch the ball (Goalie only!)
  NOOP,       // Do nothing
  QUIT        // Special action to quit the game
};

// An Action consists of the discreet action as well as required
// arguments (parameters).
struct Action {
  action_t action;
  float arg1;
  float arg2;
};

// Status of a HFO game
enum status_t
{
  IN_GAME,             // Game is currently active
  GOAL,                // A goal has been scored by the offense
  CAPTURED_BY_DEFENSE, // The defense has captured the ball
  OUT_OF_BOUNDS,       // Ball has gone out of bounds
  OUT_OF_TIME          // Trial has ended due to time limit
};

enum SideID {
  RIGHT = -1,
  NEUTRAL = 0,
  LEFT = 1
};

// Configuration of the HFO domain including the team names and player
// numbers for each team. This struct is populated by ParseConfig().
struct Config {
  std::string offense_team_name;
  std::string defense_team_name;
  int num_offense; // Number of offensive players
  int num_defense; // Number of defensive players
  std::vector<int> offense_nums; // Offensive player numbers
  std::vector<int> defense_nums; // Defensive player numbers
};

struct Player {
  SideID side;
  int unum;
};

class HFOEnvironment {
 public:
  HFOEnvironment();
  ~HFOEnvironment();

  // Returns a string representation of an action.
  static std::string ActionToString(Action action);

  // Get the number of parameters needed for a action.
  static int NumParams(action_t action);

  // Parse a Trainer message to populate config. Returns a bool
  // indicating if the struct was correctly parsed.
  static bool ParseConfig(const std::string& message, Config& config);

  // Connect to the server that controls the agent on the specified port.
  void connectToAgentServer(int server_port=6000,
                            feature_set_t feature_set=HIGH_LEVEL_FEATURE_SET);

  // Get the current state of the domain. Returns a reference to feature_vec.
  const std::vector<float>& getState();

  // Specify action type to take followed by parameters for that action
  virtual void act(action_t action, ...);

  // Send/receive communication from other agents
  virtual void say(const std::string& message);
  virtual std::string hear();

  // Get the current player holding the ball
  virtual Player playerOnBall();

  // Indicates the agent is done and the environment should
  // progress. Returns the game status after the step
  virtual status_t step();

 protected:
  int numFeatures; // The number of features in this domain
  int sockfd; // Socket file desriptor for connection to agent server
  std::vector<float> feature_vec; // Holds the features
  action_t requested_action; // Action requested
  std::vector<float> action_params; // Action parameters
  std::string say_msg, hear_msg; // Messages to hear/say
  Player player_on_ball;

  // Handshake with the agent server to ensure data is being correctly
  // passed. Also sets the number of features to expect.
  virtual void handshakeAgentServer(feature_set_t feature_set);
};

} // namespace hfo
#endif
