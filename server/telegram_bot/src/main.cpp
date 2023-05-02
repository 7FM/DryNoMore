#include <fstream>
#include <iostream>
#include <signal.h>
#include <thread>
#include <type_traits>
#include <yaml-cpp/yaml.h>

#include "dry_no_more_server.hpp"
#include "networking.hpp"
#include "telegram_bot.hpp"

static std::atomic<bool> running(true);

static void signalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    running.store(false);
  }
}

// This code is required to change uint8_t values to uint16_t otherwise YAML
// export and import treats the values as characters!
template <class T>
struct ____yaml_dec_type {
  typedef T type;
};
template <>
struct ____yaml_dec_type<uint8_t> {
  typedef uint16_t type;
};
template <class T>
struct yaml_dec_type {
  typedef ____yaml_dec_type<std::remove_cvref_t<T>>::type type;
};
template <class T>
using yaml_dec_type_t = typename yaml_dec_type<T>::type;
template <class T>
yaml_dec_type_t<T> yaml_encode(T t) {
  return static_cast<yaml_dec_type_t<T>>(t);
}

namespace YAML {
  template <>
  struct convert<SensConfig> {
    static Node encode(const SensConfig &conf) {
      Node node;
      node["minValue"] = yaml_encode(conf.minValue);
      node["maxValue"] = yaml_encode(conf.maxValue);
      return node;
    }

    static bool decode(const Node &node, SensConfig &conf) {
      conf.minValue =
          node["minValue"].as<yaml_dec_type_t<decltype(conf.minValue)>>();
      conf.maxValue =
          node["maxValue"].as<yaml_dec_type_t<decltype(conf.maxValue)>>();
      return true;
    }
  };
} // namespace YAML

namespace YAML {
  template <>
  struct convert<WaterLvlThresholds> {
    static Node encode(const WaterLvlThresholds &conf) {
      Node node;
      node["warnThres"] = yaml_encode(conf.warnThres);
      node["emptyThres"] = yaml_encode(conf.emptyThres);
      return node;
    }

    static bool decode(const Node &node, WaterLvlThresholds &conf) {
      conf.warnThres =
          node["warnThres"].as<yaml_dec_type_t<decltype(conf.warnThres)>>();
      conf.emptyThres =
          node["emptyThres"].as<yaml_dec_type_t<decltype(conf.emptyThres)>>();
      return true;
    }
  };
} // namespace YAML

namespace YAML {
  template <>
  struct convert<Settings> {
    static Node encode(const Settings &set) {
      Node node;

      auto sensConfNode = node["sensConf"];
      auto waterThresNode = node["waterLvlThres"];
      auto targetMoistNode = node["targetMoisture"];
      auto burstDurationNode = node["burstDuration"];
      auto burstDelayNode = node["burstDelay"];
      auto maxBurstsNode = node["maxBursts"];
      auto ticksBetweenIrrigationNode = node["ticksBetweenIrrigation"];
      auto moistSensToWaterSensBitmapNode = node["moistSensToWaterSensBitmap"];
      auto skipBitmapNode = node["skipBitmap"];

      for (const auto &w : set.waterLvlThres) {
        waterThresNode.push_back(w);
      }
      for (const auto &s : set.sensConfs) {
        sensConfNode.push_back(s);
      }
      for (unsigned i = 0; i < MAX_MOISTURE_SENSOR_COUNT; ++i) {
        targetMoistNode.push_back(yaml_encode(set.targetMoisture[i]));
        burstDurationNode.push_back(yaml_encode(set.burstDuration[i]));
        burstDelayNode.push_back(yaml_encode(set.burstDelay[i]));
        maxBurstsNode.push_back(yaml_encode(set.maxBursts[i]));
        ticksBetweenIrrigationNode.push_back(
            yaml_encode(set.ticksBetweenIrrigation[i]));

        // handle bitmaps
        bool val = (set.moistSensToWaterSensBitmap[i / 8] &
                    (static_cast<uint8_t>(1) << (i & 7 /*aka mod 8*/))) != 0;
        moistSensToWaterSensBitmapNode.push_back(yaml_encode(val));
        val = (set.skipBitmap[i / 8] &
               (static_cast<uint8_t>(1) << (i & 7 /*aka mod 8*/))) != 0;
        skipBitmapNode.push_back(yaml_encode(val));
      }
      node["numPlants"] = yaml_encode(set.numPlants);
      node["hardwareFailure"] = yaml_encode(set.hardwareFailure);
      node["debug"] = yaml_encode(set.debug);

      return node;
    }

    static bool decode(const Node &node, Settings &set) {
      // clear bitmaps
      for (auto &s : set.moistSensToWaterSensBitmap) {
        s = 0;
      }
      for (auto &s : set.skipBitmap) {
        s = 0;
      }

      const auto &sensConfNode = node["sensConf"];
      const auto &waterThresNode = node["waterLvlThres"];
      const auto &targetMoistNode = node["targetMoisture"];
      const auto &burstDurationNode = node["burstDuration"];
      const auto &burstDelayNode = node["burstDelay"];
      const auto &maxBurstsNode = node["maxBursts"];
      const auto &ticksBetweenIrrigationNode = node["ticksBetweenIrrigation"];
      const auto &moistSensToWaterSensBitmapNode =
          node["moistSensToWaterSensBitmap"];
      const auto &skipBitmapNode = node["skipBitmap"];

      unsigned i = 0;
      for (auto &w : set.waterLvlThres) {
        w = waterThresNode[i].as<yaml_dec_type_t<decltype(w)>>();
        ++i;
      }
      i = 0;
      for (auto &s : set.sensConfs) {
        s = sensConfNode[i].as<yaml_dec_type_t<decltype(s)>>();
        ++i;
      }
      for (i = 0; i < MAX_MOISTURE_SENSOR_COUNT; ++i) {
        set.targetMoisture[i] =
            targetMoistNode[i]
                .as<yaml_dec_type_t<decltype(set.targetMoisture[i])>>();
        set.burstDuration[i] =
            burstDurationNode[i]
                .as<yaml_dec_type_t<decltype(set.burstDuration[i])>>();
        set.burstDelay[i] =
            burstDelayNode[i]
                .as<yaml_dec_type_t<decltype(set.burstDelay[i])>>();
        set.maxBursts[i] =
            maxBurstsNode[i].as<yaml_dec_type_t<decltype(set.maxBursts[i])>>();
        set.ticksBetweenIrrigation[i] =
            ticksBetweenIrrigationNode[i]
                .as<yaml_dec_type_t<decltype(set.ticksBetweenIrrigation[i])>>();

        // handle bitmaps
        bool val = moistSensToWaterSensBitmapNode[i]
                       .as<yaml_dec_type_t<decltype(val)>>();
        set.moistSensToWaterSensBitmap[i / 8] |=
            val ? (1 << (i & 7 /*aka mod 8*/)) : 0;
        val = skipBitmapNode[i].as<yaml_dec_type_t<decltype(val)>>();
        set.skipBitmap[i / 8] |= val ? (1 << (i & 7 /*aka mod 8*/)) : 0;
      }
      set.numPlants =
          node["numPlants"].as<yaml_dec_type_t<decltype(set.numPlants)>>();
      set.hardwareFailure =
          node["hardwareFailure"]
              .as<yaml_dec_type_t<decltype(set.hardwareFailure)>>();
      set.debug = node["debug"].as<yaml_dec_type_t<decltype(set.debug)>>();

      return true;
    }
  };
} // namespace YAML

static void readSettings(StateWrapper &state, const YAML::Node &config) {
  const auto &settingsNode = config["lastKnownSettings"];
  const auto &settingsVersionNode = config["lastKnownSettingsVersion"];
  if (settingsNode.IsDefined() && settingsVersionNode.IsDefined() &&
      settingsVersionNode.IsScalar() &&
      settingsVersionNode.as<uint64_t>() ==
          static_cast<uint64_t>(SETTINGS_VERSION_NUM)) {
    state.settingsWrap.valid = true;
    auto &set = state.settingsWrap.settings;
    set = settingsNode.as<Settings>();
  }
}

static void writeSettings(const StateWrapper &state, YAML::Node &config) {
  if (state.settingsWrap.valid) {
    config["lastKnownSettingsVersion"] =
        static_cast<uint64_t>(SETTINGS_VERSION_NUM);
    config["lastKnownSettings"] = state.settingsWrap.settings;
  }
}

int main(int argc, char **argv) {
  // Register interrupt signal handler to stop the program
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

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

  // read settings from the yaml file!
  readSettings(state, config);

// #define DEBUG_SETTINGS
#ifdef DEBUG_SETTINGS
#warning "DEBUG settings activated, do not use in production!"
  state.settingsWrap.valid = true;
  state.settingsWrap.settings.hardwareFailure = false;
  state.settingsWrap.settings.debug = false;
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

  // transfer settings into the yaml config!
  writeSettings(state, config);

  std::ofstream out(argv[1]);
  out << config;

  return 0;
}
