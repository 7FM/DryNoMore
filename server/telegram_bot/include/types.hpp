#pragma once

#include <mutex>

#include "lan_protocol.hpp"

struct Message {
  // TODO/NOTE: this has to match the PacketType definitions in lan_protocol.hpp
  enum MessageType : uint8_t {
    INVALID = 0,
    INFO_MSG = 1,
    WARN_MSG = 2,
    ERR_MSG = 4,
    FAILURE_MSG = 8
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
