#ifndef LIBMIGRATE_CLIENTDATA_H_
#define LIBMIGRATE_CLIENTDATA_H_

class ClientData {
  int m_client_identifier;
  int m_remote_port;
  char *m_state;
  int m_state_size;
  int m_descriptor;
public:
  ClientData(int client_identifier);
  ClientData(int client_identifier, int remote_port);
  int GetClientIdentifier();
  void SetClientIdentifier(int client_identifier);
  int GetRemotePort();
  void SetRemotePort(int remote_port);
  char * GetState();
  void SetState(char *state);
  int GetStateSize();
  void SetStateSize(int state_size);
  int GetDescriptor();
  void SetDescriptor(int descriptor);
};

#endif
