#include <tgbot/tgbot.h> // https://github.com/reo7sp/tgbot-cpp

// Networking stuff
#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <signal.h>
#include <thread>

#include "lan_protocol.hpp"
#include "ts_queue.hpp"

static std::atomic<bool> running(true);

static void signalHandler(int signal) {
  if (signal == SIGINT) {
    running.store(false);
  }
}

static void printErrno() { std::cerr << std::strerror(errno) << std::endl; }

struct SocketRAII {
private:
  static bool setupSocket(int fd, uint16_t tcpPort) {
    if (fd >= 0) {
      const uint32_t opt = 1;
      if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                     reinterpret_cast<const char *>(&opt), sizeof(opt)) < 0) {
        std::cerr << "Failed setting SO_REUSEADDR on socket: ";
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
        std::cerr << "Failed binding receive socket to port " << tcpPort
                  << ": ";
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

public:
  SocketRAII(uint16_t tcpPort)
      : fd(socket(SOCK_STREAM /*TCP*/, AF_INET, 0)),
        succ(setupSocket(fd, tcpPort)) {}
  ~SocketRAII() {
    if (fd >= 0) {
      close(fd);
    }
  }

  bool good() { return succ; }

  const int fd;

private:
  const bool succ;
};

struct Message {
  // TODO/NOTE: this has to match the PacketType definitions in lan_protocol.hpp
  enum MessageType : uint8_t {
    INVALID = 0,
    INFO_MSG = 1,
    WARN_MSG = 2,
    ERR_MSG = 4
  };

  Message() : msg(), msgType(INVALID) {}

  Message(std::string &&msg, MessageType msgType)
      : msg(std::move(msg)), msgType(msgType) {}

  std::string msg;

  MessageType msgType;
};

struct StatusWrapper {
  mutable std::mutex mut;
  Status status;
  bool unpublished = false;
};
struct SettingsWrapper {
  mutable std::mutex mut;
  Settings settings;
  bool valid = false;
};

struct StateWrapper {
  StatusWrapper statusWrap;
  SettingsWrapper settingsWrap;
};

static void runDryNoMoreTelegramBot(const char *token,
                                    const std::set<std::int64_t> &userWhitelist,
                                    StateWrapper &state,
                                    ts_queue<Message> &msgQueue,
                                    const std::atomic<bool> &running) {
  TgBot::Bot bot(token);

  // TODO define commands?
  // TODO check try_locks and if they are locked, then either wait or dismiss
  // the user request with a note
  bot.getEvents().onAnyMessage([&bot,
                                &userWhitelist](TgBot::Message::Ptr message) {
    if (!userWhitelist.contains(message->from->id)) {
      bot.getApi().sendMessage(message->chat->id,
                               "Access denied! Go get your own bot! User ID: " +
                                   std::to_string(message->from->id));
      return;
    }
  });

  // TODO configure or rely on default settings?
  TgBot::TgLongPoll longPoll(bot);
  while (running.load(std::memory_order_relaxed)) {

    while (running.load(std::memory_order_relaxed) && !msgQueue.empty()) {
      if (auto msg = msgQueue.limited_wait_for_pop()) {
        // TODO send messages
        // TODO how do we get the correct chat id (only for the whitelisted
        // users?)
      }
    }

    try {
      longPoll.start();
    } catch (const TgBot::TgException &e) {
      std::cerr << "telegram bot error: " << e.what() << std::endl;
    }
  }
}

static void processDryNoMoreRequest(std::unique_ptr<uint8_t[]> &buf,
                                    StateWrapper &state,
                                    ts_queue<Message> &msgQueue, int client_fd,
                                    int readSize, int bufSize) {
  // TODO process the request!

  switch (static_cast<PacketType>(buf[0])) {
    case INFO_MSG:
    case WARN_MSG:
    case ERR_MSG: {
      Message::MessageType msgType = static_cast<Message::MessageType>(buf[0]);
      std::string str(reinterpret_cast<char *>(buf.get() + 1), readSize - 1);
      msgQueue.push(Message(std::move(str), msgType));
      // TODO forward as telegram msg
      break;
    }
    case REPORT_STATUS: {
      std::scoped_lock lock(state.statusWrap.mut);
      if (readSize == sizeof(Status) + 1) {
        // TODO Pray that the byte order is correct!
        state.statusWrap.unpublished = true;
        std::memcpy(reinterpret_cast<void *>(&state.statusWrap.status),
                    reinterpret_cast<const void *>(buf.get() + 1),
                    sizeof(state.statusWrap.status));
      } else {
        // TODO WTF? Cry a river!
      }

      break;
    }
    case REQUEST_SETTINGS: {
      std::unique_lock lock(state.settingsWrap.mut);
      if (state.settingsWrap.valid) {
        // send current settings!
        // TODO prey that the byte order is correct!
        write(client_fd,
              reinterpret_cast<const void *>(&state.settingsWrap.settings),
              sizeof(state.settingsWrap.settings));
      } else {
        // We have no settings yet! -> unlock
        lock.unlock();
        // send dummy response as indication that we want to receive the
        // settings ourselves!
        uint8_t dummyData = 42;
        // TODO check result
        write(client_fd, reinterpret_cast<const void *>(&dummyData),
              sizeof(dummyData));

        // receive currently used settings
        // TODO check result
        readSize =
            read(client_fd, reinterpret_cast<void *>(buf.get()), bufSize);

        if (readSize == sizeof(Settings)) {
          lock.lock();
          state.settingsWrap.valid = true;
          // TODO Pray that the byte order is correct!
          std::memcpy(reinterpret_cast<void *>(&state.settingsWrap.settings),
                      reinterpret_cast<const void *>(buf.get()),
                      sizeof(state.settingsWrap.settings));
        } else {
          // TODO WTF? Cry a river!
        }
      }
      break;
    }
  }
}

static void runDryNoMoreStatusServer(int fd, StateWrapper &state,
                                     ts_queue<Message> &msgQueue,
                                     const std::atomic<bool> &running) {
// TODO use config?
#define BUF_SIZE 1024

  std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(BUF_SIZE);

  for (int client_fd; running.load(std::memory_order_relaxed);) {
    if ((client_fd = accept(fd, nullptr /*client address*/,
                            nullptr /*client address storage size*/)) < 0) {
      // TODO where would timeouts occur?
      // if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "DryNoMore status server: failed to accept client: ";
      printErrno();
      // }
    } else {
      int res =
          read(client_fd, reinterpret_cast<void *>(buf.get()), BUF_SIZE - 1);
      if (res <= 0) {
        std::cerr << "DryNoMore status server: failed to receive request from "
                     "client: ";
        printErrno();
      } else {
        buf[res] = '\0';
      }

      processDryNoMoreRequest(buf, state, msgQueue, client_fd, res, BUF_SIZE);

      // Close the connection and release the file descriptor again!
      close(client_fd);
    }
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