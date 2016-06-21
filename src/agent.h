#ifndef AGENT_H
#define AGENT_H

#include "action_generator.h"
#include "field_evaluator.h"
#include "communication.h"
#include "feature_extractor.h"
#include "common.hpp"

#include <rcsc/player/player_agent.h>
#include <vector>

class Agent : public rcsc::PlayerAgent {
public:
  Agent();
  virtual ~Agent();
  virtual FieldEvaluator::ConstPtr getFieldEvaluator() const;

  // Returns the feature extractor corresponding to the feature_set_t
  static FeatureExtractor* getFeatureExtractor(hfo::feature_set_t feature_set,
                                               int num_teammates,
                                               int num_opponents,
                                               bool playing_offense);

  inline long statusUpdateTime() { return lastStatusUpdateTime; }

  // Process incoming trainer messages. Used to update the game status.
  void ProcessTrainerMessages();
  // Process incoming teammate messages.
  void ProcessTeammateMessages();
  // Update the state features from the world model.
  void UpdateFeatures();

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

 protected:
  hfo::feature_set_t feature_set;      // Requested feature set
  FeatureExtractor* feature_extractor; // Extracts the features
  long lastTrainerMessageTime;         // Last time the trainer sent a message
  long lastTeammateMessageTime;        // Last time a teammate sent a message
  long lastStatusUpdateTime;           // Last time we got a status update
  hfo::status_t game_status;           // Current status of the game
  hfo::Player player_on_ball;          // Player in posession of the ball
  std::vector<float> state;            // Vector of current state features
  std::string say_msg, hear_msg;       // Messages to/from teammates
  hfo::action_t requested_action;      // Currently requested action
  std::vector<float> params;           // Parameters of current action

 public:
  inline const std::vector<float>& getState() { return state; }
  inline hfo::status_t getGameStatus() { return game_status; }
  inline const hfo::Player& getPlayerOnBall() { return player_on_ball; }
  inline const std::string& getHearMsg() { return hear_msg; }
  int getUnum(); // Returns the uniform number of the player

  inline void setFeatureSet(hfo::feature_set_t fset) { feature_set = fset; }
  inline std::vector<float>* mutable_params() { return &params; }
  inline void setAction(hfo::action_t a) { requested_action = a; }
  inline void setSayMsg(const std::string& message) { say_msg = message; }

 private:
  bool doPreprocess();
  bool doSmartKick();
  bool doShoot();
  bool doPass();
  bool doPassTo(int receiver);
  bool doDribble();
  bool doMove();
  bool doForceKick();
  bool doHeardPassReceive();

  Communication::Ptr M_communication;
  FieldEvaluator::ConstPtr M_field_evaluator;
  ActionGenerator::ConstPtr M_action_generator;
};

#endif
