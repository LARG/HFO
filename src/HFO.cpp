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

using namespace hfo;

void HFOEnvironment::connectToServer(feature_set_t feature_set,
                                     std::string config_dir,
                                     int uniform_number,
                                     int server_port,
                                     std::string server_addr,
                                     std::string team_name,
                                     bool play_goalie) {
  agent.setFeatureSet(feature_set);
  rcsc::PlayerConfig& config = agent.mutable_config();
  config.setConfigDir(config_dir);
  config.setPlayerNumber(uniform_number);
  config.setPort(server_port);
  config.setHost(server_addr);
  config.setTeamName(team_name);
  config.setGoalie(play_goalie);
  if (!agent.init(&client, 0, NULL)) {
    std::cerr << "Init failed" << std::endl;
    exit(1);
  }
  client.setClientMode(rcsc::BasicClient::ONLINE);
  if (!client.startAgent(&agent)) {
    std::cerr << "Unable to start agent" << std::endl;
    exit(1);
  }
  assert(client.isServerAlive() == true);
  while (agent.getState().empty()) {
    step();
  }
}

const std::vector<float>& HFOEnvironment::getState() {
  return agent.getState();
}

void HFOEnvironment::act(action_t action, ...) {
  agent.setAction(action);
  int n_args = NumParams(action);
  std::vector<float> *params = agent.mutable_params();
  params->resize(n_args);
  va_list vl;
  va_start(vl, action);
  for (int i = 0; i < n_args; ++i) {
    (*params)[i] = va_arg(vl, double);
  }
  va_end(vl);
}

void HFOEnvironment::act(action_t action, const std::vector<float>& params) {
  agent.setAction(action);
  int n_args = NumParams(action);
  assert(n_args == params.size());
  std::vector<float> *agent_params = agent.mutable_params();
  (*agent_params) = params;
}

void HFOEnvironment::say(const std::string& message) {
  agent.setSayMsg(message);
}

std::string HFOEnvironment::hear() {
  return agent.getHearMsg();
}

Player HFOEnvironment::playerOnBall() {
  return agent.getPlayerOnBall();
}

status_t HFOEnvironment::step() {
  bool end_of_trial = agent.getGameStatus() != IN_GAME;
  long start_cycle = agent.cycle();
  while ((agent.cycle() <= start_cycle)
         || (agent.getLastDecisionTime() < agent.cycle())
         || (end_of_trial && agent.getGameStatus() != IN_GAME)) {
    client.runStep(&agent);
    if (!client.isServerAlive()) {
      return SERVER_DOWN;
    }
  }
  return agent.getGameStatus();
}
