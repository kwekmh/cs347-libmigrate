#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <cstdlib>

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
  CreateAndSendSockets(client_struct, 5);

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
  }
  std::cout << "CONNECTED TERMINATED" << std::endl;
  char ch;
  std::cin >> ch;
}
