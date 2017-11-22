#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#include "libmigrate.h"

MigrationClientStructure * RegisterAndInitMigrationService(int sock, int port) {
  std::cout << "Init Migration Service" << std::endl;
  Context *context = new Context;
  MigrationClientStructure *client_struct = new MigrationClientStructure;

  client_struct->fd = sock;
  client_struct->port = port;
  client_struct->ready = false;

  client_struct->context = context;

  InitMigrationClient(client_struct);
  std::cout << "Init Migration Service Done" << std::endl;

  return client_struct;
}

int * CreateAndSendSockets(MigrationClientStructure *client_struct, int count) {
  std::cout << "CreateAndSendSockets()" << std::endl;
  pthread_mutex_lock(&client_struct->mutex);
  while (!client_struct->ready) {
    std::cout << "Waiting" << std::endl;
    pthread_cond_wait(&client_struct->ready_cond, &client_struct->mutex);
  }
  pthread_mutex_unlock(&client_struct->mutex);
  std::cout << "Ready!" << std::endl;
  int i;

  int *fds = new int[count];

  int sock;

  for (i = 0; i < count; i++) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    fds[i] = sock;
  }

  SendSocketMessages(client_struct->sock, fds, count);
  return fds;
}

void RegisterService(MigrationClientStructure* client_struct, int service_identifier) {
  std::cout << "RegisterService()" << std::endl;
  pthread_mutex_lock(&client_struct->mutex);
  while (!client_struct->ready) {
    pthread_cond_wait(&client_struct->ready_cond, &client_struct->mutex);
  }
  pthread_mutex_unlock(&client_struct->mutex);
  std::cout << "Ready to register service" << std::endl;

  std::stringstream msgstream;

  msgstream << "REG " << service_identifier;

  std::string msg = msgstream.str();

  msgstream.str("");
  msgstream.clear();

  msgstream << msg.length() << " " << msg;

  msg = msgstream.str();

  if (send(client_struct->sock, msg.c_str(), msg.length(), 0) < 0) {
    perror("RegisterService() send");
  }
}

void SendApplicationState(MigrationClientStructure *client_struct, int service_identifier, int client_identifier, char *state, size_t size) {
  std::cout << "SendApplicationState()" << std::endl;
  pthread_mutex_lock(&client_struct->mutex);
  while (!client_struct->ready) {
    pthread_cond_wait(&client_struct->ready_cond, &client_struct->mutex);
  }
  pthread_mutex_unlock(&client_struct->mutex);
  std::cout << "Ready to send application state" << std::endl;
  unsigned int i;

  std::stringstream msgstream;

  msgstream << "STATE " << service_identifier << " " << client_identifier << " ";

  for (i = 0; i < size; i++) {
    msgstream << state[i];
  }

  std::string msg = msgstream.str();

  msgstream.str("");
  msgstream.clear();

  msgstream << msg.length() << " " << msg;

  msg = msgstream.str();

  if (send(client_struct->sock, msg.c_str(), msg.length(), 0) < 0) {
    perror("SendApplicationState() send");
  }
}

void InitMigrationClient(MigrationClientStructure *client_struct) {
  int sock;
  struct sockaddr_un addr;

  if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("InitMigrationClient() sock");
  }

  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, MIGRATION_CLIENT_SOCKET_PATH, strlen(MIGRATION_CLIENT_SOCKET_PATH) + 1);

  if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    std::cout << addr.sun_path << std::endl;
    perror("InitMigrationClient() connect");
  }

  client_struct->sock = sock;

  pthread_t client_service_pthread;

  pthread_create(&client_service_pthread, NULL, HandleMigrationClientService, (void *) client_struct);
}

void * HandleMigrationClientService(void *data) {
  MigrationClientStructure *client_struct = (MigrationClientStructure *) data;
  pthread_mutex_lock(&client_struct->mutex);

  int sock = client_struct->sock;

  char buf[MSG_BUFFER_SIZE];

  int in_bytes;

  SendSocketMessage(sock, client_struct->fd);

  client_struct->ready = true;
  pthread_cond_signal(&client_struct->ready_cond);
  pthread_mutex_unlock(&client_struct->mutex);

  while (1) {
    in_bytes = recv(sock, &buf, MSG_BUFFER_SIZE - 1, 0);
    if (in_bytes < 0) {
      perror("HandleMigrationClientService() recv");
      pthread_exit(NULL);
    } else if (in_bytes == 0) {
      pthread_exit(NULL);
    }
    buf[in_bytes] = '\0';
    std::cout << "LOCALMSG: " << buf << std::endl;
    int i = 0;
    while (i < in_bytes) {
      std::stringstream msg_size_stream;

      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          msg_size_stream << buf[i];
        } else {
          break;
        }
      }

      int msg_size = std::stoi(msg_size_stream.str());
      if (strncmp(buf + i, "NEW", 3) == 0) {
        i += 4;
        int *fds_to_send = client_struct->context->fds;
        for (int i = 0; i < client_struct->context->fd_count; i++) {
          int fd = *(fds_to_send + i);
          if (!SendSocketMessage(sock, fd)) {
            std::cout << "Failed to send descriptor " << fd << std::endl;
          }
        }
      } else if (strncmp(buf + i, "REQ", 3) == 0) {
        std::stringstream service_identifier_ss;
        std::stringstream count_ss;

        int max_bytes = i + msg_size;

        for (i += 4; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            service_identifier_ss << buf[i];
          } else {
            break;
          }
        }

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            count_ss << buf[i];
          } else {
            break;
          }
        }

        int service_identifier = std::stoi(service_identifier_ss.str());
        int count = std::stoi(count_ss.str());

        int *fds = CreateAndSendSockets(client_struct, count);

        auto clients_it = client_struct->context->services.find(service_identifier);
        std::unordered_map<int, char *> *clients;
        if (clients_it == client_struct->context->services.end()) {
          clients = new std::unordered_map<int, char *>();
          client_struct->context->services[service_identifier] = clients;
        } else {
          clients = clients_it->second;
        }

        for (int i = 0; i < count; i++) {
          (*clients)[fds[i]] = NULL;
        }
      }
    }
  }
}

bool SendSocketMessage(int sock, int fd) {
  if (send(sock, "9 SOCKETS 1", 11, 0) < 0) {
    perror("SendSocketMessage() send");
    return false;
  } else {
    int fds[1];
    fds[0] = fd;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return SendSocketMessageDescriptors(sock, fds, 1);
  }
}

bool SendSocketMessages(int sock, int *fds, int fd_count) {
  std::string fd_count_str = std::to_string(fd_count);

  std::stringstream socket_msg_ss;

  socket_msg_ss << "SOCKETS " << fd_count_str;

  std::string socket_msg = socket_msg_ss.str();

  socket_msg_ss.str("");
  socket_msg_ss.clear();

  socket_msg_ss << socket_msg.length() << " " << socket_msg;

  socket_msg = socket_msg_ss.str();

  if (send(sock, socket_msg.c_str(), socket_msg.length(), 0) < 0) {
    perror("SendSocketMessages() send");
    return false;
  } else {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return SendSocketMessageDescriptors(sock, fds, fd_count);
  }
}

bool SendSocketMessageDescriptor(int sock, int fd) {
  std::cout << "Sending descriptor " << fd << std::endl;
  struct msghdr msghdr;
  char nothing = '!';
  struct iovec nothing_ptr;
  struct cmsghdr *cmsghdr;

  //char buf[SOCKET_BUFFER_MAX_SIZE];
  struct {
    struct cmsghdr h;
    int fd[1];
  } buf;

  nothing_ptr.iov_base = &nothing;
  nothing_ptr.iov_len = 1;
  msghdr.msg_name = NULL;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &nothing_ptr;
  msghdr.msg_iovlen = 1;
  msghdr.msg_flags = 0;
  msghdr.msg_control = &buf;
  msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);
  cmsghdr = CMSG_FIRSTHDR(&msghdr);
  cmsghdr->cmsg_len = msghdr.msg_controllen;
  cmsghdr->cmsg_level = SOL_SOCKET;
  cmsghdr->cmsg_type = SCM_RIGHTS;

  ((int *) CMSG_DATA(cmsghdr))[0] = fd;

  if (sendmsg(sock, &msghdr, 0) < 0) {
    perror("SendSocketMessage() sendmsg");
    return false;
  } else {
    std::cout << "Descriptor sent" << std::endl;
    return true;
  }
}

bool SendSocketMessageDescriptors(int sock, int *fds, int fd_count) {
  int i;
  std::stringstream fd_ss;
  for (i = 0; i < fd_count; i++) {
    fd_ss << fds[i];
    if (i < fd_count - 1) {
      fd_ss << ", ";
    }
  }
  std::cout << "Sending descriptors " << fd_ss.str() << std::endl;
  struct msghdr msghdr;
  char nothing = '!';
  struct iovec nothing_ptr;
  struct cmsghdr *cmsghdr;

  //char buf[SOCKET_BUFFER_MAX_SIZE];
  struct {
    struct cmsghdr h;
    int fd[SOCKET_BUFFER_MAX_SIZE];
  } buf;

  nothing_ptr.iov_base = &nothing;
  nothing_ptr.iov_len = 1;
  msghdr.msg_name = NULL;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &nothing_ptr;
  msghdr.msg_iovlen = 1;
  msghdr.msg_flags = 0;
  msghdr.msg_control = &buf;
  msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int) * fd_count;
  cmsghdr = CMSG_FIRSTHDR(&msghdr);
  cmsghdr->cmsg_len = msghdr.msg_controllen;
  cmsghdr->cmsg_level = SOL_SOCKET;
  cmsghdr->cmsg_type = SCM_RIGHTS;

  for (i = 0; i < fd_count; i++) {
    ((int *) CMSG_DATA(cmsghdr))[i] = *(fds + i);
  }

  if (sendmsg(sock, &msghdr, 0) < 0) {
    perror("SendSocketMessage() sendmsg");
    return false;
  } else {
    std::cout << "Descriptors sent" << std::endl;
    return true;
  }
}
