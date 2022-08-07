#include <iostream>
#include <signal.h>
#include <thread>

#include "dry_no_more_server.hpp"
#include "networking.hpp"
#include "telegram_bot.hpp"

static std::atomic<bool> running(true);

static void signalHandler(int signal) {
  if (signal == SIGINT) {
    running.store(false);
  }
}

int main() {
  // Register interrupt signal handler to stop the program
  signal(SIGINT, signalHandler);

  // TODO use argv or a config file?!
  const char *const token = "PLACE YOUR TOKEN HERE";
  std::set<std::int64_t> userWhitelist;
  userWhitelist.insert(-1 /*INSERT YOUR USER ID's HERE*/);
  uint16_t tcpPort = 42424 /*SELECT YOUR SERVER TCP PORT HERE*/;

  StateWrapper state;
  ts_queue<Message> msgQueue;

  SocketRAII socket(tcpPort);

  if (!socket.good()) {
    return 1;
  }

  std::thread statusServer(runDryNoMoreStatusServer, socket.fd, std::ref(state),
                           std::ref(msgQueue), std::cref(running));

  runDryNoMoreTelegramBot(token, userWhitelist, state, msgQueue, running);

  running.store(false);
  statusServer.join();

  return 0;
}