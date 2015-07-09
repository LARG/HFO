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
#include <sstream>

bool HFOEnvironment::ParseHFOConfig(const std::string& message,
                                    HFO_Config& config) {
  config.num_offense = -1;
  config.num_defense = -1;
  std::istringstream iss(message);
  std::string header = "HFO_SETUP";
  std::string key, val;
  iss >> key;
  if (header.compare(key) != 0) {
    std::cerr << "Got unexpected message header: " << header;
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

void error(const char *msg) {
  perror(msg);
  exit(0);
}

HFOEnvironment::HFOEnvironment() {}
HFOEnvironment::~HFOEnvironment() {
  // Send a quit action and close the connection to the agent's server
  action_t quit = QUIT;
  if (send(sockfd, &quit, sizeof(int), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  close(sockfd);
}

void HFOEnvironment::connectToAgentServer(int server_port,
                                          feature_set_t feature_set) {
  std::cout << "[Agent Client] Connecting to Agent Server on port "
            << server_port << std::endl;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }
  struct hostent *server = gethostbyname("localhost");
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
  struct sockaddr_in serv_addr;
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons(server_port);
  int status = -1;
  int retry = 10;
  while (status < 0 && retry > 0) {
    status = connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    sleep(1);
    retry--;
  }
  if (status < 0) {
    error("[Agent Client] ERROR Unable to communicate with server");
  }
  std::cout << "[Agent Client] Connected" << std::endl;
  handshakeAgentServer(feature_set);
  // Get the initial game state
  feature_vec.resize(numFeatures);
  if (recv(sockfd, &(feature_vec.front()), numFeatures*sizeof(float), 0) < 0) {
    error("[Agent Client] ERROR recieving state features from socket");
  }
}

void HFOEnvironment::handshakeAgentServer(feature_set_t feature_set) {
  // Recieve float 123.2345
  float f;
  if (recv(sockfd, &f, sizeof(float), 0) < 0) {
    error("[Agent Client] ERROR recv from socket");
  }
  // Check that error is within bounds
  if (abs(f - 123.2345) > 1e-4) {
    error("[Agent Client] Handshake failed. Improper float recieved.");
  }
  // Send float 5432.321
  f = 5432.321;
  if (send(sockfd, &f, sizeof(float), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  // Send the feature set request
  if (send(sockfd, &feature_set, sizeof(int), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  // Recieve the number of features
  if (recv(sockfd, &numFeatures, sizeof(int), 0) < 0) {
    error("[Agent Client] ERROR recv from socket");
  }
  if (send(sockfd, &numFeatures, sizeof(int), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  // Recieve the game status
  hfo_status_t status;
  if (recv(sockfd, &status, sizeof(hfo_status_t), 0) < 0) {
    error("[Agent Client] ERROR recv from socket");
  }
  if (status != IN_GAME) {
    std::cout << "[Agent Client] Handshake failed: status check." << std::endl;
    exit(1);
  }
  std::cout << "[Agent Client] Handshake complete" << std::endl;
}

const std::vector<float>& HFOEnvironment::getState() {
  return feature_vec;
}

hfo_status_t HFOEnvironment::act(Action action) {
  hfo_status_t game_status;
  // Send the action
  if (send(sockfd, &action, sizeof(Action), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  // Get the game status
  if (recv(sockfd, &game_status, sizeof(hfo_status_t), 0) < 0) {
    error("[Agent Client] ERROR recieving from socket");
  }
  // Get the next game state
  if (recv(sockfd, &(feature_vec.front()), numFeatures*sizeof(float), 0) < 0) {
    error("[Agent Client] ERROR recieving state features from socket");
  }
  return game_status;
}
