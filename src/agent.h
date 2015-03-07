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
  KICK    // Kick(power, direction)
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

  // Add the angle and distance to the landmark to the feature_vec
  void addLandmarkFeature(const rcsc::Vector2D& landmark,
                          const rcsc::Vector2D& self_pos);

  int numTeammates;
  int numOpponents;
  int numFeatures; // Total number of features
  // Number of features for non-player objects. Clearly this is the answer.
  const static int num_basic_features = 42;
  // Number of features for each player or opponent in game.
  const static int features_per_player = 5;
  std::vector<float> feature_vec; // Contains the current features
  int featIndx; // Feature being populated
  const static int server_port = 6008;

  // Start the server and listen for a connection.
  virtual void startServer();
  // Transmit information to the client and ensure it can recieve.
  virtual void clientHandshake();

 private:
  bool doPreprocess();
  bool doShoot();
  bool doForceKick();
  bool doHeardPassReceive();

  Communication::Ptr M_communication;
  FieldEvaluator::ConstPtr M_field_evaluator;
  ActionGenerator::ConstPtr M_action_generator;
  bool server_running; // Is the server running?
  int sockfd, newsockfd; // Server sockets
};

#endif
