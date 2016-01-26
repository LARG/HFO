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
#include "HFO.hpp"
#include "feature_extractor.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/types.h>
#include <vector>

class Agent : public rcsc::PlayerAgent {
public:
  Agent();
  virtual ~Agent();
  virtual FieldEvaluator::ConstPtr getFieldEvaluator() const;

  // Get the current game status
  static std::vector<int> getGameStatus(const rcsc::AudioSensor& audio_sensor,
                                     long& lastTrainerMessageTime);

  // Returns the feature extractor corresponding to the feature_set_t
  static FeatureExtractor* getFeatureExtractor(hfo::feature_set_t feature_set,
                                               int num_teammates,
                                               int num_opponents,
                                               bool playing_offense);

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

  // Start the server and listen for a connection.
  void startServer(int server_port=6008);
  void listenForConnection();
  // Transmit information to the client and ensure it can recieve.
  void clientHandshake();

 protected:
  FeatureExtractor* feature_extractor;
  long lastTrainerMessageTime; // Last time the trainer sent a message
  long lastTeammateMessageTime; // Last time a teammate sent a message
  int server_port; // Port to start the server on
  bool client_connected; // Has the client connected and handshake?
  int sockfd, newsockfd; // Server sockets
  int num_teammates, num_opponents;
  bool playing_offense;

 private:
  bool doPreprocess();
  bool doShoot();
  bool doPass();
  bool doDribble();
  bool doMove();
  bool doForceKick();
  bool doHeardPassReceive();

  Communication::Ptr M_communication;
  FieldEvaluator::ConstPtr M_field_evaluator;
  ActionGenerator::ConstPtr M_action_generator;
};

#endif
