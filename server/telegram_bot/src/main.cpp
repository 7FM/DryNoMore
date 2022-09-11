#include <fstream>
#include <iostream>
#include <signal.h>
#include <thread>
#include <yaml-cpp/yaml.h>

#include "dry_no_more_server.hpp"
#include "networking.hpp"
#include "telegram_bot.hpp"

static std::atomic<bool> running(true);

static void signalHandler(int signal) {
  if (signal == SIGINT) {
    running.store(false);
  }
}

int main(int argc, char **argv) {
  // Register interrupt signal handler to stop the program
  signal(SIGINT, signalHandler);

  if (argc != 2) {
    std::cout << "Usage: ./telegram_bot <config.yaml>" << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);

  const auto &tokenNode = config["token"];
  if (!tokenNode.IsDefined() || !tokenNode.IsScalar()) {
    std::cout << "Invalid or missing 'token' attribute in config file!"
              << std::endl;
    return 2;
  }

  const auto &tcpPortNode = config["tcp_port"];
  if (tokenNode.IsDefined() && !tokenNode.IsScalar()) {
    std::cout << "Invalid 'tcp_port' attribute in config file!" << std::endl;
    return 3;
  }

  const auto &whitelistNode = config["user_whitelist"];
  if (!whitelistNode.IsDefined() || !whitelistNode.IsSequence()) {
    std::cout << "Invalid or missing 'user_whitelist' attribute in config file!"
              << std::endl;
    return 4;
  }

  const auto &userChatsNode = config["user_chats"];
  if (!userChatsNode.IsDefined() || !userChatsNode.IsSequence()) {
    std::cout << "Invalid or missing 'user_chats' attribute in config file!"
              << std::endl;
    return 5;
  }

  const std::string token = tokenNode.as<std::string>();
  uint16_t tcpPort = tcpPortNode.as<uint16_t>(42424);
  std::set<std::int64_t> userWhitelist;
  {
    auto vec = whitelistNode.as<std::vector<std::int64_t>>();
    for (auto u : vec) {
      userWhitelist.insert(u);
    }
  }
  std::set<std::int64_t> broadcastChats;
  {
    auto vec = userChatsNode.as<std::vector<std::int64_t>>();
    for (auto u : vec) {
      broadcastChats.insert(u);
    }
  }

  StateWrapper state;

  //TODO read settings from yaml file!

// #define DEBUG_SETTINGS
#ifdef DEBUG_SETTINGS
#warning "DEBUG settings activated, do not use in production!"
  state.settingsWrap.valid = true;
  state.settingsWrap.settings.hardwareFailure = false;
  state.settingsWrap.settings.numPlants = 2;
  state.settingsWrap.settings.sensConfs[0].minValue = 100;
  state.settingsWrap.settings.sensConfs[0].maxValue = 500;
  state.settingsWrap.settings.sensConfs[1].minValue = 101;
  state.settingsWrap.settings.sensConfs[1].maxValue = 501;
  state.settingsWrap.settings.moistSensToWaterSensBitmap[0] = 0b010;
  state.settingsWrap.settings.skipBitmap[0] = 0b10;
  state.settingsWrap.settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + 0]
      .minValue = 100;
  state.settingsWrap.settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + 0]
      .maxValue = 500;
  state.settingsWrap.settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + 1]
      .minValue = 101;
  state.settingsWrap.settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + 1]
      .maxValue = 501;
  state.settingsWrap.settings.waterLvlThres[0].warnThres = 50;
  state.settingsWrap.settings.waterLvlThres[0].emptyThres = 10;
  state.settingsWrap.settings.waterLvlThres[1].warnThres = 49;
  state.settingsWrap.settings.waterLvlThres[1].emptyThres = 11;
#endif

  ts_queue<Message> msgQueue;

  SocketRAII socket(tcpPort);

  if (!socket.good()) {
    return 6;
  }

  std::thread statusServer(runDryNoMoreStatusServer, socket.fd, std::ref(state),
                           std::ref(msgQueue), std::cref(running));

  runDryNoMoreTelegramBot(token, userWhitelist, broadcastChats, state, msgQueue,
                          running);

  running.store(false);
  statusServer.join();

  // write back config!
  config["tcp_port"] = tcpPort;
  config.remove("user_chats");
  for (auto c : broadcastChats) {
    config["user_chats"].push_back(c);
  }

  //TODO write settings to yaml file!

  std::ofstream out(argv[1]);
  out << config;

  return 0;
}
