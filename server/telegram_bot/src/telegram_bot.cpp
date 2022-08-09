#include <iostream>
#include <sstream>
#include <tgbot/tgbot.h> // https://github.com/reo7sp/tgbot-cpp
#include <vector>

#include "telegram_bot.hpp"
#include "telegram_bot_utils.hpp"

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
