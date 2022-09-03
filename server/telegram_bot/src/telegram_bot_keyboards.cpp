#include "telegram_bot_keyboards.hpp"
#include "telegram_bot_utils.hpp"

#include "types.hpp"

#include <bit>

template <std::integral T>
constexpr T byteswap(T value) noexcept {
  static_assert(std::has_unique_object_representations_v<T>,
                "T may not have padding bits");
  auto value_representation =
      std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  std::ranges::reverse(value_representation);
  return std::bit_cast<T>(value_representation);
}

static uint16_t get16BitValue(const uint8_t *ptr) {
  uint8_t b1 = ptr[0];
  uint8_t b2 = ptr[1];
  if constexpr (std::endian::native == std::endian::little) {
    return (static_cast<uint16_t>(b2) << 8) | b1;
  } else {
    return (static_cast<uint16_t>(b1) << 8) | b2;
  }
}

static void set16BitValue(uint8_t *ptr, uint16_t value) {
  uint8_t b1 = value & 0xFF;
  uint8_t b2 = (value >> 8) & 0xFF;
  if constexpr (std::endian::native == std::endian::little) {
    ptr[0] = b1;
    ptr[1] = b2;
  } else {
    ptr[0] = b2;
    ptr[1] = b1;
  }
}

Keyboard::Keyboard(Callback callback)
    : keyboard(new TgBot::InlineKeyboardMarkup), callback(callback) {}

void Keyboard::addRow(std::shared_ptr<Keyboard> &keyboard,
                      std::vector<std::shared_ptr<Button>> &&row) {
  std::vector<TgBot::InlineKeyboardButton::Ptr> internalRow;
  internalRow.reserve(row.size());
  for (auto &b : row) {
    internalRow.push_back(b->button);
    keyboard->buttons.insert({b->button->callbackData, b});
  }

  keyboard->keyboard->inlineKeyboard.push_back(std::move(internalRow));
}

Button::Button(std::string &&text, std::string &&callbackID, Callback callback,
               std::weak_ptr<Keyboard> newKeyboard)
    : button(new TgBot::InlineKeyboardButton), callback(std::move(callback)),
      newKeyboard(std::move(newKeyboard)) {
  button->text = text;
  button->callbackData = callbackID;
}

struct EditValueInfo {
  uint8_t *value16 = nullptr;
  uint8_t *value8 = nullptr;
  uint16_t maxVal;
  uint16_t minVal;
  uint8_t idx;
  bool isWaterLvl;
};

TgBot::InlineKeyboardMarkup::Ptr KeyboardManager::init() {
  // Assummes that keyboards[0] is the topLayer keyboard
  activeKeyboard = keyboards[0];
  keyboardHistory.clear();
  return keyboards[0]->keyboard;
}

static void
updateValue(int16_t valueUpdate,
            const std::shared_ptr<EditValueInfo> &editValueInfo,
            const std::vector<std::weak_ptr<Keyboard>> &keyboardHistory,
            const TgBot::Api &api, Settings &settings,
            TgBot::CallbackQuery::Ptr query, Keyboard *currentKb) {
  int32_t value;
  if (editValueInfo->value8 != nullptr) {
    value = *(editValueInfo->value8);
  } else if (editValueInfo->value16 != nullptr) {
    value = get16BitValue(editValueInfo->value16);
  }

  // update value and write back!
  value += valueUpdate;
  value = std::max<int32_t>(std::min<int32_t>(value, editValueInfo->maxVal),
                            editValueInfo->minVal);

  if (editValueInfo->value8 != nullptr) {
    *(editValueInfo->value8) = value;
  } else if (editValueInfo->value16 != nullptr) {
    set16BitValue(editValueInfo->value16, value);
  }

  if (keyboardHistory.empty()) {
    return;
  }

  // update the message with the tables
  keyboardHistory.back().lock()->callback(api, settings, query, currentKb);
}

KeyboardManager::KeyboardManager(Settings &settings,
                                 std::function<void()> handleCommit)
    : settings(settings) {

  auto editValueInfo = std::make_shared<EditValueInfo>();

  // Keyboard definitions
  auto topLayer = std::make_shared<Keyboard>(
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *thisKb) {
        api.editMessageText(generateSettingsTable(settings),
                            query->message->chat->id, query->message->messageId,
                            query->inlineMessageId, "Markdown", false,
                            thisKb->keyboard);
      });
  keyboards.push_back(topLayer);

  const auto simpleKeyboardChange = [](const TgBot::Api &api, Settings &,
                                       TgBot::CallbackQuery::Ptr query,
                                       Keyboard *thisKb) {
    // Nothing to do here except changing the keyboard
    api.editMessageReplyMarkup(query->message->chat->id,
                               query->message->messageId,
                               query->inlineMessageId, thisKb->keyboard);
  };

  auto editValue = std::make_shared<Keyboard>(simpleKeyboardChange);
  keyboards.push_back(editValue);

  auto editPercentage = std::make_shared<Keyboard>(simpleKeyboardChange);
  keyboards.push_back(editPercentage);

  auto selectID = std::make_shared<Keyboard>(simpleKeyboardChange);
  keyboards.push_back(selectID);

  auto editMoistLayer = std::make_shared<Keyboard>(
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *thisKb) {
        api.editMessageText(generateMoistSettingsTable(settings),
                            query->message->chat->id, query->message->messageId,
                            query->inlineMessageId, "Markdown", false,
                            thisKb->keyboard);
      });
  keyboards.push_back(editMoistLayer);

  auto editMoistDetailLayer = std::make_shared<Keyboard>(
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *thisKb) {
        api.editMessageText(generateMoistSettingsTable(settings),
                            query->message->chat->id, query->message->messageId,
                            query->inlineMessageId, "Markdown", false,
                            thisKb->keyboard);
      });
  keyboards.push_back(editMoistDetailLayer);

  auto editWaterLayer = std::make_shared<Keyboard>(
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *thisKb) {
        api.editMessageText(generateWaterSettingsTable(settings),
                            query->message->chat->id, query->message->messageId,
                            query->inlineMessageId, "Markdown", false,
                            thisKb->keyboard);
      });
  keyboards.push_back(editWaterLayer);

  auto editWaterDetailLayer = std::make_shared<Keyboard>(
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *thisKb) {
        api.editMessageText(generateWaterSettingsTable(settings),
                            query->message->chat->id, query->message->messageId,
                            query->inlineMessageId, "Markdown", false,
                            thisKb->keyboard);
      });
  keyboards.push_back(editWaterDetailLayer);

  // =================================================================
  // Utility buttons
  // =================================================================

  auto backBut = std::make_shared<Button>(
      "Back", "back",
      [this](const TgBot::Api &api, Settings &settings,
             TgBot::CallbackQuery::Ptr query, Keyboard *) {
        // go back to the parent keyboard!

        if (keyboardHistory.empty()) {
          return;
        }

        // TODO do we need an additional exit keyboard callback?
        if (auto parentKb = keyboardHistory.back().lock()) {
          parentKb->callback(api, settings, query, parentKb.get());
          activeKeyboard = parentKb;
          keyboardHistory.pop_back();
        }
      });

  auto editMinValBut = std::make_shared<Button>(
      "Edit min value", "edit_min",
      [editValueInfo](const TgBot::Api &, Settings &settings,
                      TgBot::CallbackQuery::Ptr, Keyboard *) {
        // update global state
        editValueInfo->minVal = 0;
        editValueInfo->maxVal = 1023;
        editValueInfo->value8 = nullptr;
        editValueInfo->value16 = reinterpret_cast<uint8_t *>(&(
            settings
                .sensConfs[editValueInfo->idx + (editValueInfo->isWaterLvl
                                                     ? MAX_MOISTURE_SENSOR_COUNT
                                                     : 0)]
                .minValue));
      },
      editValue);
  auto editMaxValBut = std::make_shared<Button>(
      "Edit max value", "edit_max",
      [editValueInfo](const TgBot::Api &, Settings &settings,
                      TgBot::CallbackQuery::Ptr, Keyboard *) {
        // update global state
        editValueInfo->minVal = 0;
        editValueInfo->maxVal = 1023;
        editValueInfo->value8 = nullptr;
        editValueInfo->value16 = reinterpret_cast<uint8_t *>(&(
            settings
                .sensConfs[editValueInfo->idx + (editValueInfo->isWaterLvl
                                                     ? MAX_MOISTURE_SENSOR_COUNT
                                                     : 0)]
                .maxValue));
      },
      editValue);

  auto plus1 = std::make_shared<Button>(
      "+1", "plus_1",
      [editValueInfo, this](const TgBot::Api &api, Settings &settings,
                            TgBot::CallbackQuery::Ptr query,
                            Keyboard *currentKb) {
        updateValue(1, editValueInfo, keyboardHistory, api, settings, query,
                    currentKb);
      });
  auto plus10 = std::make_shared<Button>(
      "+10", "plus_10",
      [editValueInfo, this](const TgBot::Api &api, Settings &settings,
                            TgBot::CallbackQuery::Ptr query,
                            Keyboard *currentKb) {
        updateValue(10, editValueInfo, keyboardHistory, api, settings, query,
                    currentKb);
      });
  auto plus100 = std::make_shared<Button>(
      "+100", "plus_100",
      [editValueInfo, this](const TgBot::Api &api, Settings &settings,
                            TgBot::CallbackQuery::Ptr query,
                            Keyboard *currentKb) {
        updateValue(100, editValueInfo, keyboardHistory, api, settings, query,
                    currentKb);
      });

  auto min1 = std::make_shared<Button>(
      "-1", "min_1",
      [editValueInfo, this](const TgBot::Api &api, Settings &settings,
                            TgBot::CallbackQuery::Ptr query,
                            Keyboard *currentKb) {
        updateValue(-1, editValueInfo, keyboardHistory, api, settings, query,
                    currentKb);
      });
  auto min10 = std::make_shared<Button>(
      "-10", "min_10",
      [editValueInfo, this](const TgBot::Api &api, Settings &settings,
                            TgBot::CallbackQuery::Ptr query,
                            Keyboard *currentKb) {
        updateValue(-10, editValueInfo, keyboardHistory, api, settings, query,
                    currentKb);
      });
  auto min100 = std::make_shared<Button>(
      "-100", "min_100",
      [editValueInfo, this](const TgBot::Api &api, Settings &settings,
                            TgBot::CallbackQuery::Ptr query,
                            Keyboard *currentKb) {
        updateValue(-100, editValueInfo, keyboardHistory, api, settings, query,
                    currentKb);
      });

  const auto nop = [](const TgBot::Api &, Settings &, TgBot::CallbackQuery::Ptr,
                      Keyboard *) {
    // Nothing to do here!
  };

  // =================================================================
  // Top layer buttons
  // =================================================================

  auto editPlantBut = std::make_shared<Button>(
      "Edit plant settings", "edit_plant",
      [editMoistLayer](const TgBot::Api &, Settings &settings,
                       TgBot::CallbackQuery::Ptr, Keyboard *) {
        editMoistLayer->keyboard->inlineKeyboard[0].clear();

        // Add only active plants
        for (unsigned i = 1; i <= settings.numPlants; ++i) {
          std::string butName = "edit_p" + std::to_string(i);
          auto it = editMoistLayer->buttons.find(butName);
          if (it != editMoistLayer->buttons.end()) {
            editMoistLayer->keyboard->inlineKeyboard[0].push_back(
                it->second->button);
          }
        }
      },
      editMoistLayer);
  auto editWaterBut = std::make_shared<Button>(
      "Edit water settings", "edit_water", nop, editWaterLayer);
  auto addPlantBut = std::make_shared<Button>(
      "Add plant", "add_plant",
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *currentKb) {
        if (settings.numPlants < MAX_MOISTURE_SENSOR_COUNT) {
          ++settings.numPlants;
          // init plant settings with sane values
          settings.sensConfs[settings.numPlants - 1].minValue = 0;
          settings.sensConfs[settings.numPlants - 1].maxValue = 0;
          settings.targetMoistures[settings.numPlants - 1] = 0;
          // Skip by default
          settings.skipBitmap[(settings.numPlants - 1) / 8] |=
              (static_cast<uint8_t>(1)
               << ((settings.numPlants - 1) & 7 /*aka mod 8*/));
          // use W1 by default
          settings.moistSensToWaterSensBitmap[(settings.numPlants - 1) / 8] &=
              ~(static_cast<uint8_t>(1)
                << ((settings.numPlants - 1) & 7 /*aka mod 8*/));
          // update the message with the tables
          currentKb->callback(api, settings, query, currentKb);
        }
      });
  auto removePlantBut = std::make_shared<Button>(
      "Remove plant", "remove_plant",
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *currentKb) {
        if (settings.numPlants > 0) {
          --settings.numPlants;
          // update the message with the tables
          currentKb->callback(api, settings, query, currentKb);
        }
      });
  auto clearHardwareFailure = std::make_shared<Button>(
      "Clear HW-failure", "clear_hw_failure",
      [](const TgBot::Api &api, Settings &settings,
         TgBot::CallbackQuery::Ptr query, Keyboard *currentKb) {
        if (settings.hardwareFailure) {
          settings.hardwareFailure = false;
          // update the message with the tables
          currentKb->callback(api, settings, query, currentKb);
        }
      });
  auto commitBut = std::make_shared<Button>(
      "Commit settings", "commit_settings",
      [this, handleCommit](const TgBot::Api &api, Settings &,
                           TgBot::CallbackQuery::Ptr query, Keyboard *) {
        activeKeyboard.reset();
        keyboardHistory.clear();

        // remove inline keyboard
        api.editMessageReplyMarkup(query->message->chat->id,
                                   query->message->messageId,
                                   query->inlineMessageId);
        handleCommit();
      });
  auto abortBut = std::make_shared<Button>(
      "Abort", "abort",
      [this](const TgBot::Api &api, Settings &, TgBot::CallbackQuery::Ptr query,
             Keyboard *) {
        activeKeyboard.reset();
        keyboardHistory.clear();

        // TODO show old settings? or delete message?

        // remove inline keyboard
        api.editMessageReplyMarkup(query->message->chat->id,
                                   query->message->messageId,
                                   query->inlineMessageId);
      });

  Keyboard::addRow(topLayer, {editPlantBut, editWaterBut});
  Keyboard::addRow(topLayer, {addPlantBut, removePlantBut});
  Keyboard::addRow(topLayer, {clearHardwareFailure});
  Keyboard::addRow(topLayer, {commitBut, abortBut});

  // =================================================================
  // Edit Water settings buttons
  // =================================================================

  auto editW1But = std::make_shared<Button>(
      "Edit W1", "edit_w1",
      [editValueInfo](const TgBot::Api &, Settings &, TgBot::CallbackQuery::Ptr,
                      Keyboard *) {
        editValueInfo->idx = 0;
        editValueInfo->isWaterLvl = true;
      },
      editWaterDetailLayer);

  auto editW2But = std::make_shared<Button>(
      "Edit W2", "edit_w2",
      [editValueInfo](const TgBot::Api &, Settings &, TgBot::CallbackQuery::Ptr,
                      Keyboard *) {
        editValueInfo->idx = 1;
        editValueInfo->isWaterLvl = true;
      },
      editWaterDetailLayer);

  Keyboard::addRow(editWaterLayer, {editW1But, editW2But});
  Keyboard::addRow(editWaterLayer, {backBut});

  auto editWarnThresBut = std::make_shared<Button>(
      "Edit warn thres", "edit_warn_thres",
      [editValueInfo](const TgBot::Api &, Settings &settings,
                      TgBot::CallbackQuery::Ptr, Keyboard *) {
        if (editValueInfo->isWaterLvl) {
          editValueInfo->minVal = 0;
          editValueInfo->maxVal = 99;
          editValueInfo->value16 = nullptr;
          editValueInfo->value8 =
              &(settings.waterLvlThres[editValueInfo->idx].warnThres);
        } else {
          // TODO wtf
        }
      },
      editPercentage);

  auto editEmptyThresBut = std::make_shared<Button>(
      "Edit empty thres", "edit_empty_thres",
      [editValueInfo](const TgBot::Api &, Settings &settings,
                      TgBot::CallbackQuery::Ptr, Keyboard *) {
        if (editValueInfo->isWaterLvl) {
          editValueInfo->minVal = 0;
          editValueInfo->maxVal = 99;
          editValueInfo->value16 = nullptr;
          editValueInfo->value8 =
              &(settings.waterLvlThres[editValueInfo->idx].emptyThres);
        } else {
          // TODO wtf
        }
      },
      editPercentage);

  Keyboard::addRow(editWaterDetailLayer, {editMinValBut, editMaxValBut});
  Keyboard::addRow(editWaterDetailLayer, {editWarnThresBut, editEmptyThresBut});
  Keyboard::addRow(editWaterDetailLayer, {backBut});

  // =================================================================
  // Edit Moist settings buttons
  // =================================================================

  {
    std::vector<std::shared_ptr<Button>> buttons;
    buttons.reserve(MAX_MOISTURE_SENSOR_COUNT);
    for (unsigned i = 0; i < MAX_MOISTURE_SENSOR_COUNT; ++i) {
      auto but = std::make_shared<Button>(
          "Edit P" + std::to_string(i + 1), "edit_p" + std::to_string(i + 1),
          [editValueInfo, i](const TgBot::Api &, Settings &,
                             TgBot::CallbackQuery::Ptr, Keyboard *) {
            editValueInfo->idx = i;
            editValueInfo->isWaterLvl = false;
          },
          editMoistDetailLayer);
      buttons.push_back(but);
    }

    Keyboard::addRow(editMoistLayer, std::move(buttons));
    Keyboard::addRow(editMoistLayer, {backBut});
  }

  auto editTargetMoistBut = std::make_shared<Button>(
      "Edit target moisture", "edit_target_moist",
      [editValueInfo](const TgBot::Api &, Settings &settings,
                      TgBot::CallbackQuery::Ptr, Keyboard *) {
        if (!editValueInfo->isWaterLvl) {
          editValueInfo->minVal = 0;
          editValueInfo->maxVal = 99;
          editValueInfo->value16 = nullptr;
          editValueInfo->value8 =
              &(settings.targetMoistures[editValueInfo->idx]);
        } else {
          // TODO wtf
        }
      },
      editPercentage);

  auto editTicksBwIrrigation = std::make_shared<Button>(
      "Edit ticks between irrigation", "edit_ticks_bw_irrigation",
      [editValueInfo](const TgBot::Api &, Settings &settings,
                      TgBot::CallbackQuery::Ptr, Keyboard *) {
        if (!editValueInfo->isWaterLvl) {
          editValueInfo->minVal = 0;
          editValueInfo->maxVal = 255;
          editValueInfo->value16 = nullptr;
          editValueInfo->value8 =
              &(settings.ticksBetweenIrrigation[editValueInfo->idx]);
        } else {
          // TODO wtf
        }
      },
      editValue);

  auto toggleWaterSensMappingBut = std::make_shared<Button>(
      "Toggle water sens", "toggle_water_sens",
      [editValueInfo](const TgBot::Api &api, Settings &settings,
                      TgBot::CallbackQuery::Ptr query, Keyboard *currentKb) {
        settings.moistSensToWaterSensBitmap[editValueInfo->idx / 8] ^=
            (static_cast<uint8_t>(1) << (editValueInfo->idx & 7 /*aka mod 8*/));

        // update the message with the tables
        currentKb->callback(api, settings, query, currentKb);
      });

  auto toggleSkipBut = std::make_shared<Button>(
      "Toggle skip", "toggle_skip",
      [editValueInfo](const TgBot::Api &api, Settings &settings,
                      TgBot::CallbackQuery::Ptr query, Keyboard *currentKb) {
        settings.skipBitmap[editValueInfo->idx / 8] ^=
            (static_cast<uint8_t>(1) << (editValueInfo->idx & 7 /*aka mod 8*/));

        // update the message with the tables
        currentKb->callback(api, settings, query, currentKb);
      });

  Keyboard::addRow(editMoistDetailLayer, {editTargetMoistBut});
  Keyboard::addRow(editMoistDetailLayer, {editTicksBwIrrigation});
  Keyboard::addRow(editMoistDetailLayer,
                   {toggleWaterSensMappingBut, toggleSkipBut});
  Keyboard::addRow(editMoistDetailLayer, {editMinValBut, editMaxValBut});
  Keyboard::addRow(editMoistDetailLayer, {backBut});

  // =================================================================
  // Edit value buttons
  // =================================================================

  Keyboard::addRow(editValue, {min1, min10, min100, plus100, plus10, plus1});
  Keyboard::addRow(editValue, {backBut});

  // =================================================================
  // Edit percentage buttons
  // =================================================================

  Keyboard::addRow(editPercentage, {min1, min10, plus10, plus1});
  Keyboard::addRow(editPercentage, {backBut});

  // =================================================================
  // Create query callback functions
  // =================================================================

  // Register actions
  for (auto &kb : keyboards) {
    Keyboard::Callback onCallbackQuery =
        [this, kb](const TgBot::Api &api, Settings &settings,
                   TgBot::CallbackQuery::Ptr query, Keyboard *) {
          auto it = kb->buttons.find(query->data);
          if (it != kb->buttons.end()) {
            it->second->callback(api, settings, query, kb.get());
            if (auto newKb = it->second->newKeyboard.lock()) {
              newKb->callback(api, settings, query, newKb.get());

              // update the message with the tables
              keyboardHistory.push_back(activeKeyboard);
              activeKeyboard = newKb;
            }
          } else {
            std::cerr << "Telegram bot: ERROR found no callback handler for: "
                      << query->data << std::endl;
          }
        };
    kb->onCallbackQuery = onCallbackQuery;
  }
}

void KeyboardManager::registerActions(const TgBot::Api &api,
                                      TgBot::EventBroadcaster &broadcaster) {
  broadcaster.onCallbackQuery([&](TgBot::CallbackQuery::Ptr query) {
    api.answerCallbackQuery(query->id);

    if (auto kb = activeKeyboard.lock()) {
      kb->onCallbackQuery(api, settings, query, kb.get());
    }
  });
}
