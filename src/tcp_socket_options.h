#ifndef LIBMIGRATE_TCP_SOCKET_OPTIONS_H_
#define LIBMIGRATE_TCP_SOCKET_OPTIONS_H_

#include <stdint.h>
#include <string>

class TcpSocketOptions {
  uint32_t m_mss_clamp;
  uint32_t m_snd_wscale;
  uint32_t m_rcv_wscale;
  uint32_t m_timestamp;

public:
  TcpSocketOptions();
  TcpSocketOptions(int fd);
  uint32_t GetMssClamp();
  void SetMssClamp(uint32_t mss_clamp);
  uint32_t GetSndWscale();
  void SetSndWscale(uint32_t snd_wscale);
  uint32_t GetRcvWscale();
  void SetRcvWscale(uint32_t rcv_wscale);
  uint32_t GetTimestamp();
  void SetTimestamp(uint32_t timestamp);
  void Dump();
  std::string GetString();
};

#endif
