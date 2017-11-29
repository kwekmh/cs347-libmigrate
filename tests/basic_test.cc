#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>

#include "libmigrate.h"

int main() {
  int sock;

  struct sockaddr_in addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("sock error");
  }

  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(12345);

  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("bind error");
  }

  listen(sock, 5);

  MigrationClientStructure *client_struct = RegisterAndInitMigrationService(sock, 12345);
  //CreateAndSendSockets(client_struct, 5);
  char *state = "AAAAABBBBB";
  //RegisterService(client_struct, 10000);
  //std::this_thread::sleep_for(std::chrono::seconds(2));
  //RegisterService(client_struct, 11000);
  //std::this_thread::sleep_for(std::chrono::seconds(2))
  //SendApplicationState(client_struct, 12000, 58, state, 10);
  //SendApplicationState(client_struct, 13000, 58, state, 10);
  //SendApplicationState(client_struct, 14000, 58, state, 10);
  //SendApplicationState(client_struct, 14000, 68, state, 10);

  int client_sock;

  struct sockaddr_in client_addr;
  socklen_t client_addrlen = sizeof(client_addr);

  client_sock = accept(sock, (struct sockaddr *) &client_addr, &client_addrlen);

  int in_bytes;
  char buf[256];

  while (1) {
    in_bytes = recv(client_sock, buf, 255, 0);
    if (in_bytes < 0) {
      perror("recv error");
      exit(1);
    } else if (in_bytes == 0) {
      break;
    }
    buf[in_bytes] = '\0';
    std::cout << "RECEIVED MESSAGE: " << buf << std::endl;
    /*
    uint32_t sequence_number_send = GetSequenceNumber(client_sock, TCP_SEND_QUEUE);
    uint32_t sequence_number_recv = GetSequenceNumber(client_sock, TCP_RECV_QUEUE);
    std::cout << "Current sequence numbers: " << sequence_number_send << ", " << sequence_number_recv << std::endl;
    */
    std::stringstream ss;
    ss << in_bytes << " " << std::string(buf, in_bytes);
    std::string data = ss.str();
    SendApplicationStateWithTcp(client_struct, 12345, 1, client_sock, data.c_str(), data.length());
  }
  std::cout << "CONNECTED TERMINATED" << std::endl;
  char ch;
  std::cin >> ch;
}
