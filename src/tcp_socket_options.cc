#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <iostream>
#include <sstream>
#include <string>

#include "tcp_socket_options.h"

TcpSocketOptions::TcpSocketOptions() {
}

TcpSocketOptions::TcpSocketOptions(int fd) {
  int aux_on = 1;
  int aux_off = 0;

  uint32_t mss_clamp;
  socklen_t mss_clamp_len = sizeof(mss_clamp);

  struct tcp_info info;
  socklen_t info_len = sizeof(info);

  uint32_t timestamp;
  socklen_t timestamp_len = sizeof(timestamp);

  // Put socket into repair mode
  setsockopt(fd, SOL_TCP, TCP_REPAIR, &aux_on, sizeof(aux_on));

  getsockopt(fd, SOL_TCP, TCP_MAXSEG, &mss_clamp, &mss_clamp_len);
  getsockopt(fd, SOL_TCP, TCP_INFO, &info, &info_len);
  getsockopt(fd, SOL_TCP, TCP_TIMESTAMP, &timestamp, &timestamp_len);

  this->m_mss_clamp = m_mss_clamp;
  this->m_snd_wscale = info.tcpi_snd_wscale;
  this->m_rcv_wscale = info.tcpi_rcv_wscale;
  this->m_timestamp = timestamp;

  // Turn off repair mode
  setsockopt(fd, SOL_TCP, TCP_REPAIR, &aux_off, sizeof(aux_off));
}

uint32_t TcpSocketOptions::GetMssClamp() {
  return this->m_mss_clamp;
}

void TcpSocketOptions::SetMssClamp(uint32_t mss_clamp) {
  this->m_mss_clamp = mss_clamp;
}

uint32_t TcpSocketOptions::GetSndWscale() {
  return this->m_snd_wscale;
}

void TcpSocketOptions::SetSndWscale(uint32_t snd_wscale) {
  this->m_snd_wscale = snd_wscale;
}

uint32_t TcpSocketOptions::GetRcvWscale() {
  return this->m_rcv_wscale;
}

void TcpSocketOptions::SetRcvWscale(uint32_t rcv_wscale) {
  this->m_rcv_wscale = rcv_wscale;
}

uint32_t TcpSocketOptions::GetTimestamp() {
  return this->m_timestamp;
}

void TcpSocketOptions::SetTimestamp(uint32_t timestamp) {
  this->m_timestamp = timestamp;
}

void TcpSocketOptions::Dump() {
  std::cout << "mss_clamp: " << this->m_mss_clamp << " snd_wscale: " << this->m_snd_wscale << " rcv_wscale: " << this->m_rcv_wscale << " timestamp: " << this->m_timestamp << std::endl;
}

std::string TcpSocketOptions::GetString() {
  std::stringstream ss;

  ss << this->m_mss_clamp << " " << this->m_snd_wscale << " " << this->m_rcv_wscale << " " << this->m_timestamp;

  return ss.str();
}
