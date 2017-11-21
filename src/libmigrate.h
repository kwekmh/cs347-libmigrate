#ifndef MIGRATELIB_MIGRATELIB_H_
#define MIGRATELIB_MIGRATELIB_H_

#include <pthread.h>

typedef struct Context {
  int *fds;
  int fd_count;
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
void CreateAndSendSockets(MigrationClientStructure *client_struct, int count);
void SendApplicationState(MigrationClientStructure *client_struct, int service_identifier, int client_identifier, char *state, size_t size);
void InitMigrationClient(MigrationClientStructure *client_struct);
void * HandleMigrationClientService(void *data);
bool SendSocketMessage(int sock, int fd);
bool SendSocketMessages(int sock, int *fd, int fd_count);
bool SendSocketMessageDescriptor(int sock, int fd);
bool SendSocketMessageDescriptors(int sock, int *fds, int fd_count);

#endif
