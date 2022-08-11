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

#define DEBUG_SETTINGS // TODO remove
#ifdef DEBUG_SETTINGS
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
    return 1;
  }

  std::thread statusServer(runDryNoMoreStatusServer, socket.fd, std::ref(state),
                           std::ref(msgQueue), std::cref(running));

  runDryNoMoreTelegramBot(token, userWhitelist, state, msgQueue, running);

  running.store(false);
  statusServer.join();

  return 0;
}