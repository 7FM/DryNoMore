// Networking stuff
#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "networking.hpp"

void printErrno() { std::cerr << std::strerror(errno) << std::endl; }

bool SocketRAII::setupSocket(int fd, uint16_t tcpPort) {
  if (fd >= 0) {
    const uint32_t opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   reinterpret_cast<const char *>(&opt), sizeof(opt)) < 0) {
      std::cerr << "Failed setting SO_REUSEADDR | SO_REUSEPORT on socket: ";
      goto socketError;
    }

    struct sockaddr_in address;
    std::fill_n(reinterpret_cast<uint8_t *>(&address), sizeof(address),
                static_cast<uint8_t>(0));
    address.sin_family = AF_INET;
    address.sin_port = htons(tcpPort);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, reinterpret_cast<const struct sockaddr *>(&address),
             sizeof(address)) < 0) {
      std::cerr << "Failed binding receive socket to port " << tcpPort << ": ";
      goto socketError;
    }

    // Set default receive timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char *>(&timeout),
                   sizeof(timeout)) < 0) {
      std::cerr << "Error setting receive timeout: ";
      goto socketError;
    }

    if (listen(fd, 0 /*backlog*/) < 0) {
      std::cerr << "Socket listen failed: ";
      goto socketError;
    }
  } else {
    std::cerr << "Failed to open a tcp socket: ";
  socketError:
    printErrno();
    return false;
  }
  return true;
}

SocketRAII::SocketRAII(uint16_t tcpPort)
    : fd(socket(AF_INET, SOCK_STREAM /*TCP*/, 0)),
      succ(setupSocket(fd, tcpPort)) {}
SocketRAII::~SocketRAII() {
  if (fd >= 0) {
    close(fd);
  }
}

bool SocketRAII::good() { return succ; }
