#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>

// Networking stuff
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

#include "dry_no_more_server.hpp"
#include "networking.hpp"

// #define DEBUG_PRINTS

#ifdef DEBUG_PRINTS
class IosFlagSaver {
public:
  explicit IosFlagSaver(std::ostream &_ios) : ios(_ios), f(_ios.flags()) {}
  ~IosFlagSaver() { ios.flags(f); }

  IosFlagSaver(const IosFlagSaver &rhs) = delete;
  IosFlagSaver &operator=(const IosFlagSaver &rhs) = delete;

private:
  std::ostream &ios;
  std::ios::fmtflags f;
};
#endif

static void processDryNoMoreRequest(std::unique_ptr<uint8_t[]> &buf,
                                    StateWrapper &state,
                                    ts_queue<Message> &msgQueue, int client_fd,
                                    int readSize, int bufSize) {
  switch (static_cast<PacketType>(buf[0])) {
    case FAILURE_MSG: {
      {
        // Set the hardware failure flag!
        std::scoped_lock lock(state.settingsWrap.mut);
        state.settingsWrap.settings.hardwareFailure = true;
      }
      // Intentional fall through to also send the failure message!
    } /* fall through */
    case INFO_MSG:
    case WARN_MSG:
    case ERR_MSG: {
      // forward as telegram msg
      Message::MessageType msgType = static_cast<Message::MessageType>(buf[0]);
      std::string str(reinterpret_cast<char *>(buf.get() + 1), readSize - 1);
      msgQueue.push(Message(std::move(str), msgType));

#ifdef DEBUG_PRINTS
      std::cout << "Received *_MSG request." << std::endl;
      std::cout << "  MessageType: " << msgType << std::endl
                << "  Message: " << str << std::endl;
#endif
      break;
    }
    case REPORT_STATUS: {
#ifdef DEBUG_PRINTS
      std::cout << "Received REPORT_STATUS request." << std::endl;
#endif
      if (readSize == sizeof(Status) + 1) {
        // Only issue a status update iff the moisture of at least one plant
        // changed or we are in debug mode!
        bool changed;
        {
          std::scoped_lock lock(state.settingsWrap.mut);
          changed =
              state.settingsWrap.valid && state.settingsWrap.settings.debug;
        }
        if (!changed) {
          uint8_t numPlants =
              std::min(*(buf.get() + 1 + offsetof(Status, numPlants)),
                       static_cast<uint8_t>(MAX_MOISTURE_SENSOR_COUNT));
          const uint8_t *moistBefore =
              buf.get() + 1 + offsetof(Status, beforeMoistureLevels);
          const uint8_t *moistAfter =
              buf.get() + 1 + offsetof(Status, afterMoistureLevels);
          for (uint8_t i = 0; i < numPlants; ++i) {
            if (moistBefore[i] < moistAfter[i]) {
              changed = true;
              break;
            }
          }
        }
        if (changed) {
          std::scoped_lock lock(state.statusWrap.mut);
          state.statusWrap.unpublished = true;
          std::memcpy(reinterpret_cast<void *>(&state.statusWrap.status),
                      reinterpret_cast<const void *>(buf.get() + 1),
                      sizeof(state.statusWrap.status));
        }
      } else {
        std::cerr << "Unexpected Status packet size of " << readSize
                  << " instead of " << (sizeof(Status) + 1) << std::endl;
      }

      break;
    }
    case REQUEST_SETTINGS: {
#ifdef DEBUG_PRINTS
      std::cout << "Received REQUEST_SETTINGS request." << std::endl;
#endif
      std::unique_lock lock(state.settingsWrap.mut);
      if (state.settingsWrap.valid) {
        // send current settings!
        int res =
            write(client_fd,
                  reinterpret_cast<const void *>(&state.settingsWrap.settings),
                  sizeof(state.settingsWrap.settings));
        if (res <= 0) {
          std::cerr << "DryNoMore status server: writing settings failed: ";
          printErrno();
        } else if (res != sizeof(state.settingsWrap.settings)) {
          std::cerr << "DryNoMore status server: WARNING written only " << res
                    << "/" << sizeof(state.settingsWrap.settings)
                    << " of the settings bytes!" << std::endl;
#ifdef DEBUG_PRINTS
        } else {
          std::cout << "Successfully sent settings! Bytes:" << std::endl;
          IosFlagSaver s(std::cout);
          for (unsigned i = 0; i < sizeof(state.settingsWrap.settings); ++i) {
            std::cout << "  " << std::hex
                      << static_cast<unsigned>(
                             reinterpret_cast<const uint8_t *>(
                                 &state.settingsWrap.settings)[i])
                      << std::endl;
          }
#endif
        }
      } else {
        // We have no settings yet! -> unlock
        lock.unlock();
#ifdef DEBUG_PRINTS
        std::cout << "Settings are not valid, try receiving them from the MCU."
                  << std::endl;
#endif
        // send dummy response as indication that we want to receive the
        // settings ourselves!
        uint8_t dummyData = 42;
        int res = write(client_fd, reinterpret_cast<const void *>(&dummyData),
                        sizeof(dummyData));
        if (res <= 0) {
          std::cerr << "DryNoMore status server: dummy write failed: ";
          printErrno();
          return;
        }

        // receive currently used settings
        readSize =
            read(client_fd, reinterpret_cast<void *>(buf.get()), bufSize);

        if (readSize == sizeof(Settings)) {
          lock.lock();
          state.settingsWrap.valid = true;
          std::memcpy(reinterpret_cast<void *>(&state.settingsWrap.settings),
                      reinterpret_cast<const void *>(buf.get()),
                      sizeof(state.settingsWrap.settings));
        } else {
          std::cerr << "Unexpected Settings packet size of " << readSize
                    << " instead of " << (sizeof(Settings)) << std::endl;
        }
      }
      break;
    }
  }
}

void runDryNoMoreStatusServer(int fd, StateWrapper &state,
                              ts_queue<Message> &msgQueue,
                              const std::atomic<bool> &running) {
// TODO use config?
#define BUF_SIZE 1024

  std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(BUF_SIZE);

  for (int client_fd; running.load(std::memory_order_relaxed);) {
    if ((client_fd = accept(fd, nullptr /*client address*/,
                            nullptr /*client address storage size*/)) < 0) {
      if (errno != EAGAIN /*&& errno != EWOULDBLOCK*/) {
        std::cerr << "DryNoMore status server: failed to accept client: ";
        printErrno();
      }
    } else {
      for (int res = 0;
           (res = read(client_fd, reinterpret_cast<void *>(buf.get()),
                       BUF_SIZE - 1));) {

        if (res < 0) {
          std::cerr
              << "DryNoMore status server: failed to receive request from "
                 "client: ";
          printErrno();
          break;
        } else {
          buf[res] = '\0';
        }

        processDryNoMoreRequest(buf, state, msgQueue, client_fd, res, BUF_SIZE);
      }

      // Close the connection and release the file descriptor again!
      close(client_fd);
    }
  }
}
