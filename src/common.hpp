#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

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

// Status of a HFO game
enum status_t
{
  IN_GAME,             // Game is currently active
  GOAL,                // A goal has been scored by the offense
  CAPTURED_BY_DEFENSE, // The defense has captured the ball
  OUT_OF_BOUNDS,       // Ball has gone out of bounds
  OUT_OF_TIME,         // Trial has ended due to time limit
  SERVER_DOWN          // Server is not alive
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

enum SideID {
  RIGHT = -1,
  NEUTRAL = 0,
  LEFT = 1
};

// A Player is described by its uniform number and side
struct Player {
  SideID side;
  int unum;
};

/**
 * Returns the number of parameters required for each action.
 */
inline int NumParams(const action_t action) {
 switch (action) {
   case DASH:
     return 2;
   case TURN:
     return 1;
   case TACKLE:
     return 1;
   case KICK:
     return 2;
   case KICK_TO:
     return 3;
   case MOVE_TO:
     return 2;
   case DRIBBLE_TO:
     return 2;
   case INTERCEPT:
     return 0;
   case MOVE:
     return 0;
   case SHOOT:
     return 0;
   case PASS:
     return 1;
   case DRIBBLE:
     return 0;
   case CATCH:
     return 0;
   case NOOP:
     return 0;
   case QUIT:
     return 0;
 }
 std::cerr << "Unrecognized Action: " << action << std::endl;
 return -1;
};

/**
 * Returns a string representation of an action.
 */
inline std::string ActionToString(action_t action) {
  switch (action) {
    case DASH:
      return "Dash";
    case TURN:
      return "Turn";
    case TACKLE:
      return "Tackle";
    case KICK:
      return "Kick";
    case KICK_TO:
      return "KickTo";
    case MOVE_TO:
      return "MoveTo";
    case DRIBBLE_TO:
      return "DribbleTo";
    case INTERCEPT:
      return "Intercept";
    case MOVE:
      return "Move";
    case SHOOT:
      return "Shoot";
    case PASS:
      return "Pass";
    case DRIBBLE:
      return "Dribble";
    case CATCH:
      return "Catch";
    case NOOP:
      return "No-op";
    case QUIT:
      return "Quit";
    default:
      return "Unknown";
  }
};

/**
 * Returns a string representation of a game_status.
 */
inline std::string StatusToString(status_t status) {
  switch (status) {
    case IN_GAME:
      return "InGame";
    case GOAL:
      return "Goal";
    case CAPTURED_BY_DEFENSE:
      return "CapturedByDefense";
    case OUT_OF_BOUNDS:
      return "OutOfBounds";
    case OUT_OF_TIME:
      return "OutOfTime";
    case SERVER_DOWN:
      return "ServerDown";
    default:
      return "Unknown";
  }
};

/**
 * Parse a Trainer message to populate config. Returns a bool
 * indicating if the struct was correctly parsed.
 */
inline bool ParseConfig(const std::string& message, Config& config) {
  config.num_offense = -1;
  config.num_defense = -1;
  std::istringstream iss(message);
  std::string header = "HFO_SETUP";
  std::string key, val;
  iss >> key;
  if (header.compare(key) != 0) {
    return false;
  }
  while (iss >> key) {
    if (key.compare("offense_name") == 0) {
      iss >> config.offense_team_name;
    } else if (key.compare("defense_name") == 0) {
      iss >> config.defense_team_name;
    } else if (key.compare("num_offense") == 0) {
      iss >> val;
      config.num_offense = strtol(val.c_str(), NULL, 0);
    } else if (key.compare("num_defense") == 0) {
      iss >> val;
      config.num_defense = strtol(val.c_str(), NULL, 0);
    } else if (key.compare("offense_nums") == 0) {
      assert(config.num_offense >= 0);
      for (int i=0; i<config.num_offense; ++i) {
        iss >> val;
        config.offense_nums.push_back(strtol(val.c_str(), NULL, 0));
      }
    } else if (key.compare("defense_nums") == 0) {
      assert(config.num_defense >= 0);
      for (int i=0; i<config.num_defense; ++i) {
        iss >> val;
        config.defense_nums.push_back(strtol(val.c_str(), NULL, 0));
      }
    } else {
      std::cerr << "Unrecognized key: " << key << std::endl;
      return false;
    }
  }
  assert(config.offense_nums.size() == config.num_offense);
  assert(config.defense_nums.size() == config.num_defense);
  return true;
};

/**
 * Parse a trainer message to extract the player on the ball
 */
inline bool ParsePlayerOnBall(const std::string& message, Player& player) {
  if (message.find("GOAL") != std::string::npos){
    player.unum = atoi((message.substr(message.find("-")+1)).c_str());
    player.side = LEFT;
  } else if (message.find("CAPTURED_BY_DEFENSE") != std::string::npos) {
    player.unum = atoi((message.substr(message.find("-")+1)).c_str());
    player.side = RIGHT;
  } else if (message.find("IN_GAME") != std::string::npos){
    switch (message.at(message.find("-")+1)){
      case 'L':
        player.side = LEFT;
        break;
      case 'R':
        player.side = RIGHT;
        break;
      case 'U':
        player.side = NEUTRAL;
        break;
    }
    player.unum = atoi((message.substr(message.find("-")+2)).c_str());
  } else {
    return false;
  }
  return true;
};

/**
 * Parse a trainer message to extract the game status
 */
inline bool ParseGameStatus(const std::string& message, status_t& status) {
  status = IN_GAME;
  if (message.find("GOAL") != std::string::npos){
    status = GOAL;
  } else if (message.find("CAPTURED_BY_DEFENSE") != std::string::npos) {
    status = CAPTURED_BY_DEFENSE;
  } else if (message.compare("OUT_OF_BOUNDS") == 0) {
    status = OUT_OF_BOUNDS;
  } else if (message.compare("OUT_OF_TIME") == 0) {
    status = OUT_OF_TIME;
  } else if (message.find("IN_GAME") != std::string::npos){
    status = IN_GAME;
  } else if (message.find("HFO_FINISHED") != std::string::npos){
    status = SERVER_DOWN;
  } else {
    return false;
  }
  return true;
};

}
#endif
