#ifndef MIGRATELIB_MIGRATELIB_H_
#define MIGRATELIB_MIGRATELIB_H_

#include <pthread.h>
#include <unordered_map>
#include <stdint.h>
#include <string>

#include "clientdata.h"

typedef struct Context {
  int *fds;
  int fd_count;
  std::unordered_map<int, std::unordered_map<int, ClientData *> *> services;
} Context;

typedef struct MigrationClientStructure {
  int sock;
  int port;
  int fd;
  bool ready;
  pthread_mutex_t mutex;
  pthread_cond_t ready_cond;
  Context *context;
} MigrationClientStructure;

#define STR_VALUE(arg) #arg
#define MSG_BUFFER_SIZE 256
#define SOCKET_BUFFER_MAX_SIZE 960
#define MIGRATION_CLIENT_SOCKET_PATH STR_VALUE(/var/migrated/local-socket)

MigrationClientStructure * RegisterAndInitMigrationService(int sock, int port);
int * CreateAndSendSockets(MigrationClientStructure *client_struct, int count);
void RegisterService(MigrationClientStructure *client_struct, int service_identifier);
void RegisterClient(MigrationClientStructure *client_struct, int service_identifier, int client_identifier);
void SendApplicationStateWithTcp(MigrationClientStructure *client_struct, int service_identifier, int client_identifier, int sock, const char *app_data, size_t app_data_size);
void SendApplicationState(MigrationClientStructure *client_struct, int service_identifier, int client_identifier, const char *state, size_t size);
void BuildTcpData(char **data_ptr, int *len, std::string ip_str, int remote_port, unsigned int send_seq, unsigned int recv_seq, const char *app_data, int app_data_len);
void InitMigrationClient(MigrationClientStructure *client_struct);
void * HandleMigrationClientService(void *data);
bool SendSocketMessage(int sock, int fd);
bool SendSocketMessages(int sock, int *fd, int fd_count);
bool SendSocketMessageDescriptor(int sock, int fd);
bool SendSocketMessageDescriptors(int sock, int *fds, int fd_count);
uint32_t GetSequenceNumber(int sock, int q_id);

#endif
