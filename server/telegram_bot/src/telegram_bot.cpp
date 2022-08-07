#include <iostream>
#include <sstream>
#include <tgbot/tgbot.h> // https://github.com/reo7sp/tgbot-cpp
#include <vector>

#include "telegram_bot.hpp"

static std::string
generateTable(const std::vector<std::vector<std::string>> &table) {

  if (table.empty()) {
    return "";
  }

  std::vector<size_t> maxChars;
  maxChars.reserve(table.size());

  size_t totalWidth = 1;
  for (size_t i = 0; i < table[0].size(); ++i) {
    size_t max = 0;
    for (auto &row : table) {
      max = std::max(max, row[i].size());
    }
    totalWidth += max + 3;
    maxChars.push_back(max);
  }

  std::stringstream ss;
  ss << "```\n";

  auto print_row = [&](const std::vector<std::string> &row) {
    ss << "|";
    for (size_t i = 0; i < maxChars.size(); ++i) {
      ss << ' ';
      auto old = ss.width(maxChars[i]);
      ss << row[i];
      ss.width(old);
      ss << " |";
    }
    ss << '\n';
  };

  auto print_break = [&] {
    ss.width(totalWidth);
    ss.fill('-');
    ss << '-' << '\n';
    ss.fill(' ');
  };

  ss.setf(std::ios::left, std::ios::adjustfield);
  print_break();
  print_row(table[0]);
  print_break();
  for (size_t i = 1; i < table.size(); ++i) {
    print_row(table[i]);
  }
  print_break();

  ss << "```";
  return ss.str();
}

static std::string generateSettingsTable(const Settings &settings) {
  std::vector<std::vector<std::string>> table;
  table.reserve(1 + settings.numPlants);

  std::vector<std::string> row{"Num", "Min Value", "Max Value",
                               "Target Moisture", "Water Sensor"};
  table.push_back(std::move(row));

  for (int i = 0; i < settings.numPlants; ++i) {
    uint8_t sensorIdx =
        (settings.moistSensToWaterSensBitmap[i / 8] >> (i & 7 /*aka mod 8*/)) &
        0x01;
    std::string waterSensor = sensorIdx ? "W2" : "W1";
    row = {"P" + std::to_string(i + 1),
           std::to_string(settings.sensConfs[i].minValue),
           std::to_string(settings.sensConfs[i].maxValue),
           std::to_string(settings.targetMoistures[i]) + " %", waterSensor};
    table.push_back(std::move(row));
  }

  std::string moistureSensorTable(generateTable(table));

  table.clear();

  row = {"Num", "Min Value", "Max Value", "Warning Threshold",
         "Empty Threshold"};
  table.push_back(std::move(row));

  for (int i = 0; i < 2; ++i) {
    row = {"W" + std::to_string(i + 1),
           std::to_string(
               settings.sensConfs[MAX_MOISTURE_SENSOR_COUNT + i].minValue),
           std::to_string(
               settings.sensConfs[MAX_MOISTURE_SENSOR_COUNT + i].maxValue),
           std::to_string(settings.waterLvlThres[i].warnThres) + " %",
           std::to_string(settings.waterLvlThres[i].emptyThres) + " %"};
    table.push_back(std::move(row));
  }

  std::string waterSensorTable(generateTable(table));

  return "Moisture Sensor Settings:\n" + moistureSensorTable +
         "\n\nWater-level Sensor Settings:\n" + waterSensorTable;
}

static std::string generateStatusTable(const Status &status) {
  std::vector<std::vector<std::string>> table;
  table.reserve(1 + status.numPlants);

  std::vector<std::string> row{"Num", "Moisture Before", "Moisture After"};
  table.push_back(std::move(row));

  for (int i = 0; i < status.numPlants; ++i) {
    row = {"P" + std::to_string(i + 1),
           std::to_string(status.beforeMoistureLevels[i]) + " %",
           std::to_string(status.afterMoistureLevels[i]) + " %"};
    table.push_back(std::move(row));
  }

  std::string moistureSensorTable(generateTable(table));
  table.clear();

  row = {"Num", "WaterLvl Before", "WaterLvl After"};
  table.push_back(std::move(row));

  for (int i = 0; i < status.numWaterSensors; ++i) {
    row = {"W" + std::to_string(i + 1),
           std::to_string(status.beforeWaterLevels[i]) + " %",
           std::to_string(status.afterWaterLevels[i]) + " %"};
    table.push_back(std::move(row));
  }

  std::string waterSensorTable(generateTable(table));

  return "Plant Status:\n" + moistureSensorTable + "\n\nWater-level Status:\n" +
         waterSensorTable;
}

void runDryNoMoreTelegramBot(const char *token,
                             const std::set<std::int64_t> &userWhitelist,
                             StateWrapper &state, ts_queue<Message> &msgQueue,
                             const std::atomic<bool> &running) {
  TgBot::Bot bot(token);

  std::vector<TgBot::BotCommand::Ptr> commands;
  std::vector<TgBot::EventBroadcaster::MessageListener> commandHandler;
  // TODO similar to the userWhitelist we need to persist the chats to
  // broadcast messages to!
  std::set<std::int64_t> broadcastChats;
  Settings settingSnapshot;
  auto &api = bot.getApi();

  auto addCommand = [&](const std::string &command,
                        const std::string &description,
                        TgBot::EventBroadcaster::MessageListener handler) {
    TgBot::BotCommand::Ptr cmd(new TgBot::BotCommand);
    cmd->command = command;
    cmd->description = description;
    commands.push_back(std::move(cmd));
    commandHandler.push_back(std::move(handler));
  };

  // TODO idea for updating the settings:
  //   1. Command to start editing
  //   -> use settings snapshot
  //   -> make adjustments via inline keyboard
  //   -> commit changes
  //   -> atomic settings update -> will be used on next wakeup

  // TODO build menus -> maybe create a tree structure first and automatically
  // generate required keyboards and callbacks?

  TgBot::InlineKeyboardMarkup::Ptr editSettingsKeyboard(
      new TgBot::InlineKeyboardMarkup);
  std::vector<TgBot::InlineKeyboardButton::Ptr> row0;
  TgBot::InlineKeyboardButton::Ptr but(new TgBot::InlineKeyboardButton);
  but->text = "edit sensor settings";
  but->callbackData = "edit_sensor";
  row0.push_back(but);
  but = TgBot::InlineKeyboardButton::Ptr(new TgBot::InlineKeyboardButton);
  but->text = "edit thresholds";
  but->callbackData = "edit_thres";
  row0.push_back(but);
  editSettingsKeyboard->inlineKeyboard.push_back(row0);

  std::vector<TgBot::InlineKeyboardButton::Ptr> row1;
  TgBot::InlineKeyboardButton::Ptr commitButton(
      new TgBot::InlineKeyboardButton);
  commitButton->text = "commit changes";
  commitButton->callbackData = "commit_data";
  row1.push_back(commitButton);
  editSettingsKeyboard->inlineKeyboard.push_back(row1);

  addCommand("start", "init conversation", [&](TgBot::Message::Ptr message) {
    api.sendMessage(message->chat->id, "Howdy!");
    // save the chat ID!
    broadcastChats.insert(message->chat->id);
  });

  addCommand("edit", "edit the DryNoMore irrigation settings",
             [&](TgBot::Message::Ptr message) {
               const auto &set = state.settingsWrap;
               bool valid = false;
               {
                 std::scoped_lock lock(set.mut);
                 if ((valid = set.valid)) {
                   std::memcpy(&settingSnapshot, &set.settings,
                               sizeof(settingSnapshot));
                 }
               }

               if (valid) {
                 api.sendMessage(message->chat->id,
                                 generateSettingsTable(settingSnapshot), false,
                                 0, editSettingsKeyboard, "Markdown");
               } else {
                 std::string response =
                     "Settings are not yet synchronized!\nPlease wait for the "
                     "next status report!";
                 api.sendMessage(message->chat->id, response);
               }
             });

  // Register callbacks!
  auto &broadcaster = bot.getEvents();

  broadcaster.onCallbackQuery([&](TgBot::CallbackQuery::Ptr query) {
    api.answerCallbackQuery(query->id);

    if (StringTools::startsWith(query->data, "check_data")) {
      std::string response = "ok";
      api.sendMessage(query->message->chat->id, response, false, 0,
                      editSettingsKeyboard, "Markdown");
    }
  });

  for (size_t i = 0; i < commands.size(); ++i) {
    broadcaster.onCommand(
        commands[i]->command, [&, i](TgBot::Message::Ptr message) {
          if (!userWhitelist.contains(message->from->id)) {
            api.sendMessage(message->chat->id,
                            "Access denied! Go get your own bot! User ID: " +
                                std::to_string(message->from->id));
            return;
          }

          commandHandler[i](message);
        });
  }

  // Set my commands
  api.setMyCommands(commands);

  // TODO configure or rely on default settings?
  TgBot::TgLongPoll longPoll(bot);
  while (running.load(std::memory_order_relaxed)) {

    // send all messages in the message queue
    while (running.load(std::memory_order_relaxed) && !msgQueue.empty()) {
      if (auto msg = msgQueue.limited_wait_for_pop()) {
        const char *msgPrefix = nullptr;
        switch (msg->msgType) {
          case INFO_MSG: {
            msgPrefix = "INFO: ";
            break;
          }
          case WARN_MSG: {
            msgPrefix = "WARNING: ";
            break;
          }
          case ERR_MSG: {
            msgPrefix = "ERROR: ";
            break;
          }
          default: {
            std::cerr << "Error invalid message in msgQueue!" << std::endl;
            continue;
          }
        }
        std::string message = msgPrefix + msg->msg;

        for (std::int64_t chat : broadcastChats) {
          api.sendMessage(chat, message);
        }
      }
    }

    // check if the status was not yet published & send update
    {
      std::unique_lock lock(state.statusWrap.mut);

      if (state.statusWrap.unpublished) {
        state.statusWrap.unpublished = false;
        Status statusCopy;
        std::memcpy(&statusCopy, &state.statusWrap.status, sizeof(statusCopy));
        lock.unlock();

        std::string statusUpdate = generateStatusTable(statusCopy);
        for (std::int64_t chat : broadcastChats) {
          api.sendMessage(chat, statusUpdate);
        }
      }
    }

    // Handle user input
    try {
      longPoll.start();
    } catch (const TgBot::TgException &e) {
      std::cerr << "telegram bot error: " << e.what() << std::endl;
    }
  }
}
