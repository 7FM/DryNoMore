#pragma once

void printErrno();

struct SocketRAII {
private:
  static bool setupSocket(int fd, uint16_t tcpPort);

public:
  SocketRAII(uint16_t tcpPort);
  ~SocketRAII();

  bool good();

  const int fd;

private:
  const bool succ;
};

