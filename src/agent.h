// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef AGENT_H
#define AGENT_H

#include "action_generator.h"
#include "field_evaluator.h"
#include "communication.h"

#include <rcsc/player/player_agent.h>
#include <vector>

// The actions available to the agent
enum action_t
{
  DASH,   // Dash(power, relative_direction)
  TURN,   // Turn(direction)
  TACKLE, // Tackle(direction)
  KICK,   // Kick(power, direction)
  QUIT    // Special action to quit the game
};

// The current status of the HFO game
enum hfo_status_t
{
  IN_GAME,
  GOAL,
  CAPTURED_BY_DEFENSE,
  OUT_OF_BOUNDS,
  OUT_OF_TIME
};

struct Action {
  action_t action;
  float arg1;
  float arg2;
};

class Agent : public rcsc::PlayerAgent {
public:
  Agent();
  virtual ~Agent();
  virtual FieldEvaluator::ConstPtr getFieldEvaluator() const;

protected:
  // You can override this method. But you must call
  // PlayerAgent::initImpl() in this method.
  virtual bool initImpl(rcsc::CmdLineParser& cmd_parser);

  // main decision
  virtual void actionImpl();

  // communication decision
  virtual void communicationImpl();
  virtual void handleActionStart();
  virtual void handleActionEnd();
  virtual void handleServerParam();
  virtual void handlePlayerParam();
  virtual void handlePlayerType();
  virtual FieldEvaluator::ConstPtr createFieldEvaluator() const;
  virtual ActionGenerator::ConstPtr createActionGenerator() const;

  // Updated the state features stored in feature_vec
  void updateStateFeatures();

  // Get the current game status
  hfo_status_t getGameStatus();

  // Encodes an angle feature as the sin and cosine of that angle,
  // effectively transforming a single angle into two features.
  void addAngFeature(const rcsc::AngleDeg& ang);

  // Encodes a proximity feature which is defined by a distance as
  // well as a maximum possible distance, which acts as a
  // normalizer. Encodes the distance as [0-far, 1-close]. Ignores
  // distances greater than maxDist or less than 0.
  void addDistFeature(float dist, float maxDist);

  // Add the angle and distance to the landmark to the feature_vec
  void addLandmarkFeatures(const rcsc::Vector2D& landmark,
                           const rcsc::Vector2D& self_pos,
                           const rcsc::AngleDeg& self_ang);

  // Add features corresponding to another player.
  void addPlayerFeatures(rcsc::PlayerObject& player,
                         const rcsc::Vector2D& self_pos,
                         const rcsc::AngleDeg& self_ang);

  // Start the server and listen for a connection.
  void startServer(int server_port=6008);

  // Transmit information to the client and ensure it can recieve.
  void clientHandshake();

 protected:
  int numTeammates; // Number of teammates in HFO
  int numOpponents; // Number of opponents in HFO
  bool playingOffense; // Are we playing offense or defense?
  int numFeatures; // Total number of features
  // Number of features for non-player objects.
  const static int num_basic_features = 58;
  // Number of features for each player or opponent in game.
  const static int features_per_player = 8;
  int featIndx; // Feature being populated
  std::vector<float> feature_vec; // Contains the current features

  // Observed values of some parameters.
  const static float observedSelfSpeedMax   = 0.46;
  const static float observedPlayerSpeedMax = 0.75;
  const static float observedStaminaMax     = 8000.;
  const static float observedBallSpeedMax   = 3.0;
  float maxHFORadius; // Maximum possible distance in HFO playable region
  // Useful measures defined by the Server Parameters
  float pitchLength, pitchWidth, pitchHalfLength, pitchHalfWidth,
    goalHalfWidth, penaltyAreaLength, penaltyAreaWidth;
  long lastTrainerMessageTime; // Last time the trainer sent a message
  int server_port; // Port to start the server on
  bool server_running; // Is the server running?
  int sockfd, newsockfd; // Server sockets

 private:
  bool doPreprocess();
  bool doShoot();
  bool doForceKick();
  bool doHeardPassReceive();

  Communication::Ptr M_communication;
  FieldEvaluator::ConstPtr M_field_evaluator;
  ActionGenerator::ConstPtr M_action_generator;
};

#endif
