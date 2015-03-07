#include "HFO.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>

void error(const char *msg) {
  perror(msg);
  exit(0);
}

HFOEnvironment::HFOEnvironment() {}
HFOEnvironment::~HFOEnvironment() {
  close(sockfd);
}

void HFOEnvironment::connectToAgentServer(int server_port) {
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
  while (status < 0) {
    status = connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    sleep(1);
  }
  std::cout << "[Agent Client] Connected" << std::endl;
  handshakeAgentServer();
}

void HFOEnvironment::handshakeAgentServer() {
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
  // Recieve the number of features
  if (recv(sockfd, &numFeatures, sizeof(int), 0) < 0) {
    error("[Agent Client] ERROR recv from socket");
  }
  if (send(sockfd, &numFeatures, sizeof(int), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  std::cout << "[Agent Client] Handshake complete" << std::endl;
}

const std::vector<float>& HFOEnvironment::getState() {
  if (feature_vec.size() != numFeatures) {
    feature_vec.resize(numFeatures);
  }
  if (recv(sockfd, &(feature_vec.front()), numFeatures*sizeof(float), 0) < 0) {
    error("[Agent Client] ERROR recieving state features from socket");
  }
  return feature_vec;
}

float HFOEnvironment::act(Action action) {
  if (send(sockfd, &action, sizeof(Action), 0) < 0) {
    error("[Agent Client] ERROR sending from socket");
  }
  return 0.;
}
