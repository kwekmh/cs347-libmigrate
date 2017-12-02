#ifndef MIGRATELIB_MIGRATELIB_H_
#define MIGRATELIB_MIGRATELIB_H_

#include <pthread.h>
#include <unordered_map>
#include <stdint.h>
#include <string>

#include "clientdata.h"

struct Context;
struct MigrationClientStructure;

typedef struct Context Context;
typedef struct MigrationClientStructure MigrationClientStructure;

typedef void (*HANDLER_FUNCTION)(MigrationClientStructure *, ClientData *);

struct Context {
  int *fds;
  int fd_count;
  std::unordered_map<int, std::unordered_map<int, ClientData *> *> services;
  std::unordered_map<int, std::unordered_map<std::string, ClientData *> *> ip_services;
  std::unordered_map<int, HANDLER_FUNCTION> migration_handlers;
  pthread_mutex_t sock_mutexes_mutex;
  pthread_mutex_t sock_conds_mutex;
  std::unordered_map<int, pthread_mutex_t *> sock_mutexes;
  std::unordered_map<int, pthread_cond_t *> sock_conds;
};

struct MigrationClientStructure {
  int sock;
  int port;
  int fd;
  bool ready;
  pthread_mutex_t mutex;
  pthread_cond_t ready_cond;
  Context *context;
};

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
void BuildTcpData(char **data_ptr, int *len, std::string ip_str, int remote_port, unsigned int send_seq, unsigned int recv_seq, std::string tcp_opts, const char *app_data, int app_data_len);
void InitMigrationClient(MigrationClientStructure *client_struct);
void * HandleMigrationClientService(void *data);
bool SendSocketMessage(int sock, int fd);
bool SendSocketMessages(int sock, int *fd, int fd_count);
bool SendSocketMessageDescriptor(int sock, int fd);
bool SendSocketMessageDescriptors(int sock, int *fds, int fd_count);
void DumpSocketInfo(int fd);
uint32_t GetSequenceNumber(int sock, int q_id);
ClientData *GetIpClient(MigrationClientStructure *migration_client, int service_identifier, std::string ip_str);
std::string GetAppData(std::string data_str);
pthread_mutex_t * GetMutex(Context *context, int service_identifier);
pthread_cond_t * GetCond(Context *context, int service_identifier);

#endif
