// -*-c++-*-

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
  int num_teammates;                   // Number of teammates
  int num_opponents;                   // Number of opponents
  hfo::action_t last_action_with_status; // Last action with a recorded return status
  hfo::action_status_t last_action_status;  // Recorded return status of last action

 public:
  inline const std::vector<float>& getState() { return state; }
  inline hfo::status_t getGameStatus() { return game_status; }
  inline const hfo::Player& getPlayerOnBall() { return player_on_ball; }
  inline const std::string& getHearMsg() { return hear_msg; }
  int getUnum(); // Returns the uniform number of the player
  inline int getNumTeammates() { return num_teammates; }
  inline int getNumOpponents() { return num_opponents; }
  hfo::action_status_t getLastActionStatus(hfo::action_t last_action); // if last_action is correct, returns status if available

  inline void setFeatureSet(hfo::feature_set_t fset) { feature_set = fset; }
  inline std::vector<float>* mutable_params() { return &params; }
  inline void setAction(hfo::action_t a) { requested_action = a; }
  inline void setSayMsg(const std::string& message) { say_msg = message; }

 private:
  bool doPreprocess();
  hfo::action_status_t doSmartKick();
  hfo::action_status_t doShoot();
  bool doPass();
  hfo::action_status_t doPassTo(int receiver);
  hfo::action_status_t doDribble();
  hfo::action_status_t doMove();
  bool doForceKick();
  bool doHeardPassReceive();
  hfo::action_status_t doMarkPlayer(int unum);
  bool doMarkPlayerNearIndex(int near_index);
  hfo::action_status_t doReduceAngleToGoal();
  hfo::action_status_t doDefendGoal();
  hfo::action_status_t doGoToBall();
  bool doNewAction1();
  void addLastActionStatus(hfo::action_t last_action, hfo::action_status_t action_status);
  void addLastActionStatusCollision(hfo::action_t last_action, bool may_fix, bool likely_success);


  Communication::Ptr M_communication;
  FieldEvaluator::ConstPtr M_field_evaluator;
  ActionGenerator::ConstPtr M_action_generator;
};

#endif
