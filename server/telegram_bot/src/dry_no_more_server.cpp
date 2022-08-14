#include <cstring>
#include <iostream>
#include <memory>

// Networking stuff
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

#include "dry_no_more_server.hpp"
#include "networking.hpp"

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
                    << " of the settings bytes!";
        }
      } else {
        // We have no settings yet! -> unlock
        lock.unlock();
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
