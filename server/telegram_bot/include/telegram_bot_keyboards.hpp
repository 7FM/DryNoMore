#pragma once

#include <tgbot/tgbot.h> // https://github.com/reo7sp/tgbot-cpp

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Settings;
struct Button;

struct Keyboard {
  using Callback = std::function<void(const TgBot::Api &api, Settings &settings,
                                      TgBot::CallbackQuery::Ptr, Keyboard *)>;
  Keyboard(Callback callback);

  TgBot::InlineKeyboardMarkup::Ptr keyboard;
  Callback callback;

  static void addRow(std::shared_ptr<Keyboard> &keyboard,
                     std::vector<std::shared_ptr<Button>> &&row);

  // Dynamically added
  Callback onCallbackQuery;
  std::map<std::string, std::shared_ptr<Button>> buttons;
};

struct Button {
  using Callback = std::function<void(const TgBot::Api &api, Settings &settings,
                                      TgBot::CallbackQuery::Ptr, Keyboard *)>;
  Button(std::string &&text, std::string &&callbackID, Callback callback,
         std::weak_ptr<Keyboard> newKeyboard = std::weak_ptr<Keyboard>());

  TgBot::InlineKeyboardButton::Ptr button;
  Callback callback;
  std::weak_ptr<Keyboard> newKeyboard;
};

struct KeyboardManager {
  KeyboardManager(Settings &settings);

  void registerActions(const TgBot::Api &api,
                       TgBot::EventBroadcaster &broadcaster);

  TgBot::InlineKeyboardMarkup::Ptr init();

private:
  Settings &settings;
  std::vector<std::shared_ptr<Keyboard>> keyboards;
  std::vector<std::weak_ptr<Keyboard>> keyboardHistory;
  std::weak_ptr<Keyboard> activeKeyboard;
};
