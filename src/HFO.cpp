#include "HFO.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <netdb.h>
#include <iostream>
#include <stdarg.h>
#include <agent.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/player/player_config.h>
#include <rcsc/player/world_model.h>

using namespace hfo;

#define MAX_STEPS 100

HFOEnvironment::HFOEnvironment() {
  client = new rcsc::BasicClient();
  agent = new Agent();
}

HFOEnvironment::~HFOEnvironment() {
  delete client;
  delete agent;
}

void HFOEnvironment::connectToServer(feature_set_t feature_set,
                                     std::string config_dir,
                                     int server_port,
                                     std::string server_addr,
                                     std::string team_name,
                                     bool play_goalie,
                                     std::string record_dir) {
  agent->setFeatureSet(feature_set);
  rcsc::PlayerConfig& config = agent->mutable_config();
  config.setConfigDir(config_dir);
  config.setPort(server_port);
  config.setHost(server_addr);
  config.setTeamName(team_name);
  config.setGoalie(play_goalie);
  if (!record_dir.empty()) {
    config.setLogDir(record_dir);
    config.setRecord(true);
  }
  if (!agent->init(client, 0, NULL)) {
    std::cerr << "Init failed" << std::endl;
    exit(1);
  }
  client->setClientMode(rcsc::BasicClient::ONLINE);
  if (!client->startAgent(agent)) {
    std::cerr << "Unable to start agent" << std::endl;
    exit(1);
  }
  assert(client->isServerAlive() == true);
  // Do nothing until the agent begins getting state features
  while (agent->getState().empty()) {
    act(NOOP);
    agent->executeAction();
    do {
      client->runStep(agent);
    } while (agent->lastPreActionTime() < agent->currentTime());
  }
}

const std::vector<float>& HFOEnvironment::getState() {
  return agent->getState();
}

void HFOEnvironment::act(action_t action, ...) {
  agent->setAction(action);
  int n_args = NumParams(action);
  std::vector<float> *params = agent->mutable_params();
  params->resize(n_args);
  va_list vl;
  va_start(vl, action);
  for (int i = 0; i < n_args; ++i) {
    (*params)[i] = va_arg(vl, double);
  }
  va_end(vl);
}

void HFOEnvironment::act(action_t action, const std::vector<float>& params) {
  agent->setAction(action);
  int n_args = NumParams(action);
  assert(n_args == params.size());
  std::vector<float> *agent_params = agent->mutable_params();
  (*agent_params) = params;
}

void HFOEnvironment::say(const std::string& message) {
  agent->setSayMsg(message);
}

std::string HFOEnvironment::hear() {
  return agent->getHearMsg();
}

int HFOEnvironment::getUnum() {
  return agent->getUnum();
}

Player HFOEnvironment::playerOnBall() {
  return agent->getPlayerOnBall();
}

status_t HFOEnvironment::step() {
  // Agent sends action to server
  agent->executeAction();
  // Wait until server replies with new game state
  long start_cycle = agent->currentTime().cycle();
  bool cycle_advanced = false;
  bool end_of_trial = agent->getGameStatus() != IN_GAME;
  bool still_eot = false;
  int steps = 0;
  do {
    client->runStep(agent);
    if (steps++ > MAX_STEPS) {
      break;
    }
    if (!client->isServerAlive()) {
      return SERVER_DOWN;
    }
    cycle_advanced = agent->currentTime().cycle() > start_cycle;
  } while (!cycle_advanced || agent->lastPreActionTime() < agent->currentTime());
  // If the trial is over, wait until the next episode starts
  if (end_of_trial) {
    while (agent->getGameStatus() != IN_GAME) {
      act(NOOP);
      agent->executeAction();
      do {
        client->runStep(agent);
        if (!client->isServerAlive()) {
          return SERVER_DOWN;
        }
      } while (agent->lastPreActionTime() < agent->currentTime());
    }
  }
  return agent->getGameStatus();
}
