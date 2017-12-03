#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#include "libmigrate.h"
#include "tcp_socket_options.h"

MigrationClientStructure * RegisterAndInitMigrationService(int sock, int port) {
  std::cout << "Init Migration Service" << std::endl;
  Context *context = new Context;
  MigrationClientStructure *client_struct = new MigrationClientStructure;

  pthread_mutex_init(&client_struct->mutex, NULL);
  pthread_cond_init(&client_struct->ready_cond, NULL);

  client_struct->fd = sock;
  client_struct->port = port;
  client_struct->ready = false;

  client_struct->context = context;

  InitMigrationClient(client_struct);
  RegisterService(client_struct, port);
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
    std::cout << "Created socket " << sock << std::endl;
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

void RegisterClient(MigrationClientStructure *client_struct, int service_identifier, int client_identifier) {
  pthread_mutex_lock(&client_struct->mutex);
  while (!client_struct->ready) {
    pthread_cond_wait(&client_struct->ready_cond, &client_struct->mutex);
  }
  pthread_mutex_unlock(&client_struct->mutex);
  std::stringstream msgstream;

  msgstream << "CLIENT " << service_identifier << " " << client_identifier;

  std::string msg = msgstream.str();

  msgstream.str("");
  msgstream.clear();

  msgstream << msg.length() << " " << msg;
  msg = msgstream.str();

  if (send(client_struct->sock, msg.c_str(), msg.length(), 0) < 0) {
    perror("RegisterClient() send");
  }
}

void SendApplicationState(MigrationClientStructure *client_struct, int service_identifier, int client_identifier, const char *state, size_t size) {
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

void SendApplicationStateWithTcp(MigrationClientStructure *client_struct, int service_identifier, int client_identifier, int sock, const char *app_data, size_t app_data_size) {
  // Get the remote address and port
  struct sockaddr_in addr;

  socklen_t addrlen = sizeof(addr);

  char ip_addr[INET_ADDRSTRLEN];

  getpeername(sock, (struct sockaddr *) &addr, &addrlen);

  int port = ntohs(addr.sin_port);

  inet_ntop(AF_INET, &(addr.sin_addr), ip_addr, INET_ADDRSTRLEN);

  std::cout << "IP address of peer is: " << std::string(ip_addr) << std::endl;

  char *tcp_data;
  int tcp_data_len;

  // Get the TCP sequence numbers of the specified socket
  int send_seq = GetSequenceNumber(sock, TCP_SEND_QUEUE);
  int recv_seq = GetSequenceNumber(sock, TCP_RECV_QUEUE);

  TcpSocketOptions opts(sock);

  // Build a state update message with the necessary TCP header data required for repairing a socket
  BuildTcpData(&tcp_data, &tcp_data_len, std::string(ip_addr), port, send_seq, recv_seq, opts.GetString(), app_data, app_data_size);

  SendApplicationState(client_struct, service_identifier, client_identifier, tcp_data, tcp_data_len);
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

void BuildTcpData(char **data_ptr, int *len, std::string ip_str, int remote_port, unsigned int send_seq, unsigned int recv_seq, std::string tcp_opts, const char *app_data, int app_data_len) {
  std::stringstream ss;

  ss << ip_str << " " << remote_port << " " << send_seq << " " << recv_seq << " "  << tcp_opts << " " << app_data_len << " " << std::string(app_data, app_data_len);

  std::string data = ss.str();

  const char *data_buf = data.c_str();

  *data_ptr = new char[data.length()];

  for (int i = 0; i < data.length(); i++) {
    (*data_ptr)[i] = data_buf[i];
  }

  *len = data.length();
}

void * HandleMigrationClientService(void *data) {
  MigrationClientStructure *client_struct = (MigrationClientStructure *) data;
  Context *context = client_struct->context;
  pthread_mutex_lock(&client_struct->mutex);

  int sock = client_struct->sock;

  char buf[MSG_BUFFER_SIZE];

  int in_bytes;

  SendSocketMessage(sock, client_struct->fd);

  client_struct->ready = true;
  pthread_cond_signal(&client_struct->ready_cond);
  pthread_mutex_unlock(&client_struct->mutex);

  while (1) {
    in_bytes = recv(sock, &buf, MSG_BUFFER_SIZE, 0);
    if (in_bytes < 0) {
      perror("HandleMigrationClientService() recv");
      pthread_exit(NULL);
    } else if (in_bytes == 0) {
      pthread_exit(NULL);
    }
    std::cout << "LOCALMSG: " << std::string(buf, in_bytes) << std::endl;
    int i = 0;
    while (i < in_bytes) {
      std::stringstream msg_size_ss;

      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          msg_size_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      std::string msg_size_str = msg_size_ss.str();

      int msg_size = std::stoi(msg_size_ss.str());
      if (msg_size > 3 && strncmp(buf + i, "NEW", 3) == 0) {
        i += 4;
        int *fds_to_send = client_struct->context->fds;
        for (int i = 0; i < client_struct->context->fd_count; i++) {
          int fd = *(fds_to_send + i);
          if (!SendSocketMessage(sock, fd)) {
            std::cout << "Failed to send descriptor " << fd << std::endl;
          }
        }
      } else if (msg_size > 3 && strncmp(buf + i, "REQ", 3) == 0) {
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

        i++;

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
        std::unordered_map<int, ClientData *> *clients;
        if (clients_it == client_struct->context->services.end()) {
          clients = new std::unordered_map<int, ClientData *>();
          client_struct->context->services[service_identifier] = clients;
        } else {
          clients = clients_it->second;
        }

        // HARDCODED TEST
        ClientData *client_data = new ClientData(0);
        client_data->SetDescriptor(fds[0]);
        (*clients)[0] = client_data;

      } else if (msg_size > 3 && strncmp(buf + i, "MAP", 3) == 0) {
        std::stringstream service_ident_ss;
        std::stringstream client_ident_ss;
        std::stringstream fd_ss;

        int max_bytes = i + msg_size;

        for (i += 4; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            service_ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            client_ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            fd_ss << buf[i];
          } else {
            break;
          }
        }

        int service_identifier = std::stoi(service_ident_ss.str());
        int client_identifier = std::stoi(client_ident_ss.str());
        int fd = std::stoi(fd_ss.str());

        auto it = context->services.find(service_identifier);

        std::unordered_map<int, ClientData *> *clients;

        if (it != context->services.end()) {
          clients = it->second;
        } else {
          clients = new std::unordered_map<int, ClientData *>();
          context->services[service_identifier] = clients;
        }

        // Original code used with TCP Repair that has since been deprecated
        /*
        ClientData *client_data;

        auto clients_it = clients->find(client_identifier);
        if (clients_it != clients->end()) {
          client_data = clients_it->second;
        } else {
          client_data = new ClientData(client_identifier);
          (*clients)[client_identifier] = client_data;
        }

        client_data->SetDescriptor(fd);
        */
      } else if (msg_size > 5 && strncmp(buf + i, "STATE", 5) == 0) {
        std::stringstream service_ident_ss;
        std::stringstream client_ident_ss;
        std::stringstream state_ss;

        int max_bytes = i + msg_size;

        for (i += 6; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            service_ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            client_ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          state_ss << buf[i];
        }

        std::string service_ident_str = service_ident_ss.str();
        std::string client_ident_str = client_ident_ss.str();

        int service_identifier = std::stoi(service_ident_ss.str());
        int client_identifier = std::stoi(client_ident_ss.str());

        std::cout << "Received state for " << service_identifier << " " << client_identifier << std::endl;

        std::string state_str = state_ss.str();

        std::istringstream is_state(state_str);
        std::string ip_str;
        std::string port_str;
        std::string tcp_send_seq_str;
        std::string tcp_recv_seq_str;
        std::string mss_clamp_str;
        std::string snd_wscale_str;
        std::string rcv_wscale_str;
        std::string timestamp_str;
        std::string app_info_length_str;
        if (std::getline(is_state, ip_str, ' ') && std::getline(is_state, port_str, ' ') && std::getline(is_state, tcp_send_seq_str, ' ') && std::getline(is_state, tcp_recv_seq_str, ' ') && std::getline(is_state, mss_clamp_str, ' ') && std::getline(is_state, snd_wscale_str, ' ') && std::getline(is_state, rcv_wscale_str, ' ') && std::getline(is_state, timestamp_str, ' ') && std::getline(is_state, app_info_length_str, ' ')) {
          int remote_port = std::stoi(port_str);

          auto services_it = context->services.find(service_identifier);

          std::unordered_map<int, ClientData *> *clients;

          if (services_it != context->services.end()) {
            clients = services_it->second;
          } else {
            clients = new std::unordered_map<int, ClientData *>();
            context->services[service_identifier] = clients;
          }

          ClientData *client;

          auto clients_it = clients->find(client_identifier);

          if (clients_it != clients->end()) {
            client = clients_it->second;
          } else {
            client = new ClientData(client_identifier);
            (*clients)[client_identifier] = client;
          }

          const char *state_data_buf = state_str.c_str();

          char *state_data = new char[state_str.length()];

          for (int i = 0; i < state_str.length(); i++) {
            state_data[i] = state_data_buf[i];
          }

          client->SetRemotePort(remote_port);
          client->SetState(state_data);
          client->SetStateSize(state_str.length());
        }
      } else if (msg_size > 4 && strncmp(buf + i, "DONE", 4) == 0) {
        std::stringstream service_ident_ss;

        int max_bytes = i + msg_size;
        for (i += 5; i < max_bytes; i++) {
          service_ident_ss << buf[i];
        }

        int service_identifier = std::stoi(service_ident_ss.str());
        HANDLER_FUNCTION migration_handler = context->migration_handlers[service_identifier];

        std::unordered_map<int, ClientData *> *clients = context->services[service_identifier];
        for (auto it = clients->begin(); it != clients->end(); it++) {
          std::istringstream is_state(std::string(it->second->GetState(), it->second->GetStateSize()));
          std::string ip_str;
          std::string port_str;
          std::string tcp_send_seq_str;
          std::string tcp_recv_seq_str;
          std::string mss_clamp_str;
          std::string snd_wscale_str;
          std::string rcv_wscale_str;
          std::string timestamp_str;
          std::string app_info_length_str;
          std::string app_data;
          if (std::getline(is_state, ip_str, ' ') && std::getline(is_state, port_str, ' ') && std::getline(is_state, tcp_send_seq_str, ' ') && std::getline(is_state, tcp_recv_seq_str, ' ') && std::getline(is_state, mss_clamp_str, ' ') && std::getline(is_state, snd_wscale_str, ' ') && std::getline(is_state, rcv_wscale_str, ' ') && std::getline(is_state, timestamp_str, ' ') && std::getline(is_state, app_info_length_str, ' ') && std::getline(is_state, app_data)) {
            auto ip_it = context->ip_services.find(service_identifier);

            std::unordered_map<std::string, ClientData *> *ips;
            if (ip_it != context->ip_services.end()) {
              ips = ip_it->second;
            } else {
              ips = new std::unordered_map<std::string, ClientData *>();
              context->ip_services[service_identifier] = ips;
            }

            (*ips)[ip_str] = it->second;

            std::cout << "Taking over " << it->first << " " << it->second << std::endl;
            //migration_handler(client_struct, it->second);
          }
        }
      }
    }
  }
}

void DumpSocketInfo(int fd) {
  TcpSocketOptions opts(fd);

  opts.Dump();
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
    std::this_thread::sleep_for(std::chrono::seconds(5));
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
  // A special message needs to be constructed in order for sockets to be sent
  // Sending of sockets over processes involves SCM_RIGHTS
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

uint32_t GetSequenceNumber(int sock, int q_id) {
  // Set the necessary options for setsockopt
  int aux_on = 1;
  int aux_off = 0;

  // Create variables to store the data from getsockopt
  uint32_t seq_number;

  socklen_t seq_number_len = sizeof(seq_number);

  setsockopt(sock, SOL_TCP, TCP_REPAIR, &aux_on, sizeof(aux_on));

  setsockopt(sock, SOL_TCP, TCP_REPAIR_QUEUE, &q_id, sizeof(q_id));

  getsockopt(sock, SOL_TCP, TCP_QUEUE_SEQ, &seq_number, &seq_number_len);

  setsockopt(sock, SOL_TCP, TCP_REPAIR, &aux_off, sizeof(aux_off));

  return seq_number;
}

ClientData *GetIpClient(MigrationClientStructure *migration_client, int service_identifier, std::string ip_str) {
  ClientData *data = NULL;

  auto it = migration_client->context->ip_services.find(service_identifier);

  if (it != migration_client->context->ip_services.end()) {
    if (it->second->find(ip_str) != it->second->end()) {
      data = (*it->second)[ip_str];
    } else {
      std::cout << "GetIpClient() " << ip_str << " not found!" << std::endl;
    }
  } else {
    std::cout << "GetIpClient() service not found!" << std::endl;
  }

  return data;
}

std::string GetAppData(std::string data_str) {
  std::istringstream is_state(data_str);
  std::string ip_str;
  std::string port_str;
  std::string tcp_send_seq_str;
  std::string tcp_recv_seq_str;
  std::string mss_clamp_str;
  std::string snd_wscale_str;
  std::string rcv_wscale_str;
  std::string timestamp_str;
  std::string app_info_length_str;
  std::string app_data;
  if (std::getline(is_state, ip_str, ' ') && std::getline(is_state, port_str, ' ') && std::getline(is_state, tcp_send_seq_str, ' ') && std::getline(is_state, tcp_recv_seq_str, ' ') && std::getline(is_state, mss_clamp_str, ' ') && std::getline(is_state, snd_wscale_str, ' ') && std::getline(is_state, rcv_wscale_str, ' ') && std::getline(is_state, timestamp_str, ' ') && std::getline(is_state, app_info_length_str, ' ') && std::getline(is_state, app_data)) {
    return app_data;
  }

  return std::string();
}

pthread_mutex_t * GetMutex(Context *context, int service_identifier) {
  pthread_mutex_t *mutex_ptr;
  pthread_mutex_lock(&context->sock_mutexes_mutex);
  auto it = context->sock_mutexes.find(service_identifier);

  if (it != context->sock_mutexes.end()) {
    mutex_ptr = it->second;
  } else {
    mutex_ptr = new pthread_mutex_t;
    pthread_mutex_init(mutex_ptr, NULL);
    context->sock_mutexes[service_identifier] = mutex_ptr;
  }
  pthread_mutex_unlock(&context->sock_mutexes_mutex);
  return mutex_ptr;
}

pthread_cond_t * GetCond(Context *context, int service_identifier) {
  pthread_cond_t *cond_ptr;
  pthread_mutex_lock(&context->sock_conds_mutex);
  auto it = context->sock_conds.find(service_identifier);

  if (it != context->sock_conds.end()) {
    cond_ptr = it->second;
  } else {
    cond_ptr = new pthread_cond_t;
    pthread_cond_init(cond_ptr, NULL);
    context->sock_conds[service_identifier] = cond_ptr;
  }
  pthread_mutex_unlock(&context->sock_conds_mutex);

  return cond_ptr;
}
