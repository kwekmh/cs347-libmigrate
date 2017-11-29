#include <iostream>

#include "clientdata.h"

ClientData::ClientData(int client_identifier) {
  this->m_client_identifier = client_identifier;
}

ClientData::ClientData(int client_identifier, int remote_port) {
  this->m_client_identifier = client_identifier;
  this->m_remote_port = remote_port;
}

int ClientData::GetClientIdentifier() {
  return this->m_client_identifier;
}

void ClientData::SetClientIdentifier(int client_identifier) {
  this->m_client_identifier = client_identifier;
}

int ClientData::GetRemotePort() {
  return this->m_remote_port;
}

void ClientData::SetRemotePort(int remote_port) {
  this->m_remote_port = remote_port;
}

char * ClientData::GetState() {
  return this->m_state;
}

void ClientData::SetState(char *state) {
  this->m_state = state;
}

int ClientData::GetStateSize() {
  return this->m_state_size;
}

void ClientData::SetStateSize(int state_size) {
  this->m_state_size = state_size;
}

int ClientData::GetDescriptor() {
  return this->m_descriptor;
}

void ClientData::SetDescriptor(int descriptor) {
  this->m_descriptor = descriptor;
}
